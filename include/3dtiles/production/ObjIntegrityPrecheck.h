#ifndef OSG_GIS_PLUGINS_PRODUCTION_OBJ_INTEGRITY_PRECHECK_H
#define OSG_GIS_PLUGINS_PRODUCTION_OBJ_INTEGRITY_PRECHECK_H
// A-57：原始 OBJ 面索引预检（读模**之前**跑）。
//
// 为什么需要它：osgdb_obj 不是把文件里的 `f` 索引交给 osg::Geometry 当索引，而是按引用
// 把对应的 `v` 逐个拷进几何自己的坐标数组——交付时索引恒为本地 0..N-1 的合法值。所以
// 读模之后的越界守卫（SourceModel::build，A-57 的第二道防线）对"文件里写了 N+1"这类
// 缺陷结构性失明：产物顶点数反而比源 `v` 数多，多出来的那颗是源 float 流的错位窗口
// （实测取值如 -15.3588,-3.0000,13.6699 ← (-15.358791, 13.669859, 3.0)），而且它在数学
// 上有限，非有限顶点丢弃与 selfValidate 全都放行。
// 证据：F:\wb_stage0_ffbb3fe\20261026T092706\A56_ROOT_CAUSE.md（A-56/A-57 探针轮）。
//
// 边界（必须如实声明）：只扫顶层 OBJ 文本的 v/vt/vn 三个索引空间，不重新实现转换器；
// 材质、分组语义、曲线曲面、经 .osg 嵌套引用的子文件一概不管。经 mtllib 的贴图路径
// 也不管。它拦的是"引用越界"这一类确定性输入缺陷，不是 OBJ 合法性验证器。
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace osgGISPlugins {
namespace production {

// 引用所属的索引空间：'v' = 顶点、't' = vt、'n' = vn。三个空间各自独立计数，
// 一处越界不连坐其他空间。
enum class ObjRefSpace
{
    Vertex,
    Texture,
    Normal
};

struct ObjRefViolation
{
    std::uint64_t line = 0;       // 1 基物理行号（\ 续行取逻辑行首行）
    ObjRefSpace space = ObjRefSpace::Vertex;
    std::int64_t raw = 0;         // 文件里的原文写法（负数=相对引用）
    std::uint64_t resolved = 0;   // 解析后的 1 基下标
    std::uint64_t capSoFar = 0;   // 到该行位置为止该空间已声明的条数
    std::uint64_t capTotal = 0;   // 读完全文后该空间的总条数
    std::string why;              // 见 ObjPrecheckResult 里的判定说明
    std::string token;            // 该引用的原文 token（not-integer 时是唯一线索）
    std::string text;             // 该行前 80 字符，供人直接看懂
    bool hard = true;             // false = 只警告（forward-reference）
};

struct ObjPrecheckResult
{
    bool ran = false;             // 扩展名不是 obj / -objcheck off 时保持 false
    bool readable = false;        // 文件没打开就是 false，绝不伪装成"扫过且干净"
    std::uint64_t bytes = 0;
    std::uint64_t lines = 0;      // 物理行数
    std::uint64_t logicalLines = 0;
    std::uint64_t faces = 0;      // 逻辑 `f` 行数
    std::uint64_t numV = 0, numVT = 0, numVN = 0;
    std::uint64_t maxRefV = 0, maxRefVT = 0, maxRefVN = 0;   // 解析后的最大引用
    std::uint64_t checkedRefs = 0;
    std::uint64_t badFaces = 0;   // 含至少一条硬违规的面数
    std::uint64_t hardViolations = 0;
    std::uint64_t forwardRefs = 0;
    bool pendingOverflow = false; // 前向引用挂起表超上限，判定不完整
    std::vector<ObjRefViolation> violations;   // 行序确定，最多 maxViolations 条
    double seconds = 0.0;

    bool clean() const { return readable && hardViolations == 0 && !pendingOverflow; }

    // 可解释诊断（CLI 与 conversion-report.json 的 warnings 共用同一份措辞）。
    std::vector<std::string> messages() const;
};

// 单遍顺序扫描：64 KB 缓冲、原地切 token，不把整个文件 split 进内存。
// utf8Path 是项目内部统一编码的路径（工具层显式转过 UTF-8），不是当前代码页路径。
// 判定：
//   index-zero          `0` 不是合法 OBJ 索引（1 基，负数是相对引用）
//   not-integer         token 不是整数
//   beyond-total        引用 > 全文该空间总条数 —— 确定性缺陷，A-56/A-57 的形态
//   negative-underflow  相对引用回溯到第 1 条之前（负得太多）
//   forward-reference   引用 > 到此为止的条数但 <= 总数：导出器常见，只警告不拒
//   pending-overflow    挂起表溢出，无法完成判定（按硬失败处理）
// 不修改文件、不修复索引、不裁剪几何。
ObjPrecheckResult precheckObjFile(const std::string& utf8Path, std::size_t maxViolations = 64);

} // namespace production
} // namespace osgGISPlugins

#endif // !OSG_GIS_PLUGINS_PRODUCTION_OBJ_INTEGRITY_PRECHECK_H
