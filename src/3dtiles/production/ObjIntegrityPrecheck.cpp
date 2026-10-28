#include "3dtiles/production/ObjIntegrityPrecheck.h"
#include "utils/Utils.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <set>
#include <system_error>

namespace osgGISPlugins {
namespace production {
namespace {

enum SpaceIndex
{
    kVertex = 0,
    kTexture = 1,
    kNormal = 2,
    kSpaceCount = 3
};

// 挂起表上限：正索引引用大于"到此为止的条数"时还不能定性（可能是合法导出器写的前向
// 引用，也可能真越界），必须等 EOF 的总数。正常文件攒不出几条；攒到上限说明引用分布
// 本身已经不正常，此时判定不完整，按硬失败处理而不是静默丢弃。
const std::size_t kPendingCap = 4096;

// 天文数字的索引位，只为避免 int64 溢出；任何真实 OBJ 都不会有这么多 vt。
const std::int64_t kIndexSaturate = 1000000000000000000LL;

const char* spaceTag(ObjRefSpace space)
{
    switch (space)
    {
    case ObjRefSpace::Texture: return "vt";
    case ObjRefSpace::Normal:  return "vn";
    default:                   return "v";
    }
}

ObjRefSpace spaceOf(int index)
{
    switch (index)
    {
    case kTexture: return ObjRefSpace::Texture;
    case kNormal:  return ObjRefSpace::Normal;
    default:       return ObjRefSpace::Vertex;
    }
}

// 严格整数 token：不接受小数、不接受尾部垃圾（"3abc" 是缺陷，不是"3"）。
// 溢出按 kIndexSaturate 饱和——饱和值必然撞上 beyond-total，判定不会因此变松。
bool parseIndexToken(const std::string& token, std::int64_t& valueOut)
{
    std::size_t i = 0;
    bool negative = false;
    if (i < token.size() && (token[i] == '+' || token[i] == '-'))
    {
        negative = token[i] == '-';
        ++i;
    }
    if (i >= token.size()) return false;
    std::int64_t digits = 0;
    for (; i < token.size(); ++i)
    {
        if (token[i] < '0' || token[i] > '9') return false;
        if (digits > kIndexSaturate / 10) { digits = kIndexSaturate; continue; }
        digits = digits * 10 + static_cast<std::int64_t>(token[i] - '0');
    }
    valueOut = negative ? -digits : digits;
    return true;
}

std::string clip(const std::string& text)
{
    return text.size() <= 80 ? text : text.substr(0, 80) + "...";
}

// 64 KB 缓冲的顺序行读取器：既不把整个文件读进内存，也不 split 成全行数组。
class PhysicalLineReader
{
public:
    explicit PhysicalLineReader(std::istream& stream) : _stream(stream) {}

    bool next(std::string& lineOut)
    {
        for (;;)
        {
            const std::string::size_type nl = _buffer.find('\n');
            if (nl != std::string::npos)
            {
                lineOut.assign(_buffer, 0, nl);
                _buffer.erase(0, nl + 1);
                trimCarriageReturn(lineOut);
                return true;
            }
            if (_eof)
            {
                if (_buffer.empty()) return false;
                lineOut.swap(_buffer);
                _buffer.clear();
                trimCarriageReturn(lineOut);
                return true;
            }
            char chunk[65536];
            _stream.read(chunk, static_cast<std::streamsize>(sizeof(chunk)));
            const std::streamsize got = _stream.gcount();
            if (got <= 0) { _eof = true; }
            else
            {
                _bytes += static_cast<unsigned long long>(got);
                _buffer.append(chunk, static_cast<std::size_t>(got));
            }
        }
    }

    unsigned long long consumedBytes() const { return _bytes; }

private:
    static void trimCarriageReturn(std::string& line)
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
    }

    std::istream& _stream;
    std::string _buffer;
    bool _eof = false;
    unsigned long long _bytes = 0;
};

struct PendingRef
{
    std::uint64_t line = 0;
    int space = kVertex;
    std::int64_t raw = 0;
    std::uint64_t resolved = 0;
    std::uint64_t capSoFar = 0;
    std::string token;
    std::string text;
};

class ObjScanner
{
public:
    ObjScanner(ObjPrecheckResult& result, std::size_t maxViolations)
        : _res(result), _maxViolations(maxViolations)
    {
    }

    void scan(std::istream& stream)
    {
        PhysicalLineReader reader(stream);
        std::string physical;
        for (;;)
        {
            // 逻辑行 = 物理行 + 末尾反斜杠续行；行号取首行，续行的物理行照样计数。
            std::string logical;
            std::uint64_t logicalLine = 0;
            for (;;)
            {
                if (!reader.next(physical)) break;
                ++_res.lines;
                if (logicalLine == 0) logicalLine = _res.lines;
                if (endsWithBackslash(physical))
                {
                    logical += stripTrailingBackslash(physical);
                    continue;
                }
                logical += physical;
                break;
            }
            if (logicalLine == 0) break;   // EOF
            ++_res.logicalLines;
            processLine(logicalLine, logical);
        }
        _res.bytes = static_cast<std::uint64_t>(reader.consumedBytes());
        _res.numV = _counts[kVertex];
        _res.numVT = _counts[kTexture];
        _res.numVN = _counts[kNormal];
        resolvePending();
        _res.badFaces = static_cast<std::uint64_t>(_badFaceLines.size());
    }

private:
    static std::string::size_type trailingBackslash(const std::string& line)
    {
        std::size_t i = line.size();
        while (i > 0 && (line[i - 1] == ' ' || line[i - 1] == '\t')) { --i; }
        return (i > 0 && line[i - 1] == '\\') ? i - 1 : std::string::npos;
    }

    static bool endsWithBackslash(const std::string& line)
    {
        return trailingBackslash(line) != std::string::npos;
    }

    static std::string stripTrailingBackslash(const std::string& line)
    {
        const std::string::size_type at = trailingBackslash(line);
        return line.substr(0, at);
    }

    void processLine(std::uint64_t lineNumber, const std::string& rawLine)
    {
        // OBJ 允许行内 # 注释：从第一个 # 起整段丢弃（读取器同口径，多读无害）。
        std::string line = rawLine;
        const std::string::size_type hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::size_t begin = 0;
        while (begin < line.size() && (line[begin] == ' ' || line[begin] == '\t')) ++begin;
        if (begin >= line.size()) return;   // 空行/纯注释行：不参与任何计数

        const std::string::size_type kwEnd = line.find_first_of(" \t", begin);
        const std::string keyword = line.substr(begin,
            kwEnd == std::string::npos ? std::string::npos : kwEnd - begin);
        if (keyword == "v")  { ++_counts[kVertex]; return; }
        if (keyword == "vt") { ++_counts[kTexture]; return; }
        if (keyword == "vn") { ++_counts[kNormal]; return; }
        if (keyword == "f")  { parseFace(lineNumber, line, kwEnd, clip(rawLine)); return; }
        // vp / s / g / o / mtllib / usemtl / l / c_… 一律不进入三个索引空间的计数。
    }

    void parseFace(std::uint64_t lineNumber, const std::string& line,
                   std::string::size_type from, const std::string& text)
    {
        ++_res.faces;
        bool faceBad = false;
        std::string token;
        for (std::size_t i = from; ; ++i)
        {
            const bool atEnd = i >= line.size();
            const bool isSpace = atEnd || line[i] == ' ' || line[i] == '\t';
            if (!isSpace) { token += line[i]; continue; }
            if (!token.empty())
            {
                if (!parseVertexRef(lineNumber, token, text)) faceBad = true;
                token.clear();
            }
            if (atEnd) break;
        }
        if (faceBad) _badFaceLines.insert(lineNumber);
    }

    // token 形如 v | v/vt | v//vn | v/vt/vn；第 4 段（vd）不参与判定。
    bool parseVertexRef(std::uint64_t lineNumber, const std::string& token,
                        const std::string& text)
    {
        std::string fields[kSpaceCount];
        std::string::size_type pos = 0;
        int seen = 0;
        for (; seen < kSpaceCount; ++seen)
        {
            const std::string::size_type slash = token.find('/', pos);
            fields[seen] = token.substr(pos,
                slash == std::string::npos ? std::string::npos : slash - pos);
            if (slash == std::string::npos) break;
            pos = slash + 1;
        }
        bool ok = true;
        if (fields[0].empty())
        {
            record(lineNumber, kVertex, 0, 0, _counts[kVertex], token, text, "index-missing");
            ok = false;
        }
        for (int space = 0; space < kSpaceCount; ++space)
        {
            if (fields[space].empty()) continue;   // v//vn 的空 vt 段 = 不引用该空间
            std::int64_t raw = 0;
            if (!parseIndexToken(fields[space], raw))
            {
                record(lineNumber, space, 0, 0, _counts[space], fields[space], text, "not-integer");
                ok = false;
                continue;
            }
            if (!checkIndex(lineNumber, space, raw, fields[space], text)) ok = false;
        }
        return ok;
    }

    bool checkIndex(std::uint64_t lineNumber, int space, std::int64_t raw,
                    const std::string& token, const std::string& text)
    {
        const std::uint64_t declared = _counts[space];
        ++_res.checkedRefs;
        if (raw == 0)
        {
            record(lineNumber, space, raw, 0, declared, token, text, "index-zero");
            return false;
        }
        if (raw > 0)
        {
            const std::uint64_t resolved = static_cast<std::uint64_t>(raw);
            bumpMax(space, resolved);
            if (resolved <= declared) return true;
            if (_pending.size() >= kPendingCap)
            {
                _res.pendingOverflow = true;
                return true;   // 已挂起的条目照样由 resolvePending 定性
            }
            PendingRef p;
            p.line = lineNumber; p.space = space; p.raw = raw;
            p.resolved = resolved; p.capSoFar = declared;
            p.token = token; p.text = text;
            _pending.push_back(std::move(p));
            return true;       // 前向引用本身只是警告，不标坏面
        }
        // 负索引 = 相对"到此为止的条数"：resolved = declared + raw + 1。单遍正向就能判。
        const std::int64_t rel = static_cast<std::int64_t>(declared) + raw + 1;
        if (rel < 1)
        {
            record(lineNumber, space, raw, 0, declared, token, text, "negative-underflow");
            return false;
        }
        bumpMax(space, static_cast<std::uint64_t>(rel));
        return true;
    }

    void bumpMax(int space, std::uint64_t resolved)
    {
        std::uint64_t& target = space == kTexture ? _res.maxRefVT
                              : space == kNormal  ? _res.maxRefVN
                                                    : _res.maxRefV;
        if (resolved > target) target = resolved;
    }

    void record(std::uint64_t lineNumber, int space, std::int64_t raw,
                std::uint64_t resolved, std::uint64_t capSoFar,
                const std::string& token, const std::string& text, const char* why)
    {
        ++_res.hardViolations;
        if (_res.violations.size() < _maxViolations)
        {
            ObjRefViolation v;
            v.line = lineNumber;
            v.space = spaceOf(space);
            v.raw = raw;
            v.resolved = resolved;
            v.capSoFar = capSoFar;
            v.capTotal = _counts[space];
            v.why = why;
            v.token = token;
            v.text = text;
            v.hard = true;
            _res.violations.push_back(std::move(v));
        }
    }

    void resolvePending()
    {
        for (const PendingRef& p : _pending)
        {
            const std::uint64_t total = _counts[p.space];
            if (p.resolved > total)
            {
                // A-56/A-57 的确切形态：文件里引用了不存在的第 N+1 条。
                _badFaceLines.insert(p.line);
                record(p.line, p.space, p.raw, p.resolved, p.capSoFar,
                       p.token, p.text, "beyond-total");
                continue;
            }
            ++_res.forwardRefs;
            if (_res.violations.size() < _maxViolations)
            {
                ObjRefViolation v;
                v.line = p.line;
                v.space = spaceOf(p.space);
                v.raw = p.raw;
                v.resolved = p.resolved;
                v.capSoFar = p.capSoFar;
                v.capTotal = total;
                v.why = "forward-reference";
                v.token = p.token;
                v.text = p.text;
                v.hard = false;
                _res.violations.push_back(std::move(v));
            }
        }
        _pending.clear();
    }

    ObjPrecheckResult& _res;
    std::size_t _maxViolations;
    std::uint64_t _counts[kSpaceCount] = { 0, 0, 0 };
    std::vector<PendingRef> _pending;
    // 坏面按行号去重：一行一个面，一个面里多条越界引用只算一个坏面。
    std::set<std::uint64_t> _badFaceLines;
};

} // namespace

std::vector<std::string> ObjPrecheckResult::messages() const
{
    std::vector<std::string> out;
    if (!readable)
    {
        out.push_back("obj precheck: file could not be opened, index coverage is unknown");
        return out;
    }
    out.reserve(violations.size() + (pendingOverflow ? 1 : 0));
    for (const ObjRefViolation& v : violations)
    {
        const char* tag = spaceTag(v.space);
        char buffer[512];
        if (v.why == "beyond-total")
        {
            std::snprintf(buffer, sizeof(buffer),
                "line %llu: face references %s index %llu but the file declares only %llu %s "
                "entries (beyond-total); raw=\"%s\" | %s",
                static_cast<unsigned long long>(v.line), tag,
                static_cast<unsigned long long>(v.resolved),
                static_cast<unsigned long long>(v.capTotal), tag,
                v.token.c_str(), v.text.c_str());
        }
        else if (v.why == "index-zero")
        {
            std::snprintf(buffer, sizeof(buffer),
                "line %llu: %s index 0 is not a valid 1-based OBJ reference (index-zero) | %s",
                static_cast<unsigned long long>(v.line), tag, v.text.c_str());
        }
        else if (v.why == "negative-underflow")
        {
            std::snprintf(buffer, sizeof(buffer),
                "line %llu: %s index %lld points before the first entry; only %llu declared "
                "so far (negative-underflow) | %s",
                static_cast<unsigned long long>(v.line), tag,
                static_cast<long long>(v.raw),
                static_cast<unsigned long long>(v.capSoFar), v.text.c_str());
        }
        else if (v.why == "not-integer")
        {
            std::snprintf(buffer, sizeof(buffer),
                "line %llu: face element \"%s\" is not an integer index reference "
                "(not-integer) | %s",
                static_cast<unsigned long long>(v.line), v.token.c_str(), v.text.c_str());
        }
        else if (v.why == "index-missing")
        {
            std::snprintf(buffer, sizeof(buffer),
                "line %llu: face element \"%s\" has no vertex index before the first '/' "
                "(index-missing) | %s",
                static_cast<unsigned long long>(v.line), v.token.c_str(), v.text.c_str());
        }
        else
        {
            std::snprintf(buffer, sizeof(buffer),
                "line %llu: face references %s index %llu while only %llu are declared so far, "
                "%llu by end of file (forward-reference, not rejected) | %s",
                static_cast<unsigned long long>(v.line), tag,
                static_cast<unsigned long long>(v.resolved),
                static_cast<unsigned long long>(v.capSoFar),
                static_cast<unsigned long long>(v.capTotal), v.text.c_str());
        }
        out.push_back(buffer);
    }
    if (pendingOverflow)
    {
        out.push_back("obj precheck: forward-reference table overflowed; index coverage "
                      "is incomplete (pending-overflow)");
    }
    return out;
}

ObjPrecheckResult precheckObjFile(const std::string& utf8Path, std::size_t maxViolations)
{
    ObjPrecheckResult result;
    result.ran = true;
    result.badFaces = 0;
    // 走 Utils::utf8ToPath：本参数是项目内部统一的 UTF-8 路径，而窄字符 fstream 入口
    // 按 C locale 代码页解码同一个字符串，中文资产目录会被解成"文件不存在"（A-59）。
    std::ifstream in(Utils::utf8ToPath(utf8Path), std::ios::binary);
    if (!in)
    {
        result.readable = false;
        return result;
    }
    result.readable = true;
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    ObjScanner scanner(result, maxViolations);
    scanner.scan(in);
    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    result.seconds = std::chrono::duration<double>(end - start).count();
    return result;
}

} // namespace production
} // namespace osgGISPlugins
