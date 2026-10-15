#ifndef OSG_GIS_PLUGINS_PRODUCTION_TRACE_H
#define OSG_GIS_PLUGINS_PRODUCTION_TRACE_H
// 生产级管线的调试追踪（只写 stderr，且每帧 flush）
//
// 打开方式：设置环境变量 WB_TRACE=1。
// 为什么需要它：本项目的运行环境下，进程在 main 返回之后的静态析构阶段会被
// 宿主以 0xC0000409 结束，std::cout 的块缓冲来不及冲刷，重定向到管道/文件时会
// 丢失全部日志。stderr 是无缓冲的，因此追踪信息一律走 stderr。
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

namespace osgGISPlugins {
namespace production {

inline bool traceEnabled()
{
    static const int on = (std::getenv("WB_TRACE") != nullptr) ? 1 : 0;
    return on != 0;
}

inline void traceLine(const std::string& s)
{
    std::fprintf(stderr, "[trace] %s\n", s.c_str());
    std::fflush(stderr);
}

} // namespace production
} // namespace osgGISPlugins

#define WB_TRACE_ENABLED() ::osgGISPlugins::production::traceEnabled()

#define TRACE(msg)                                                            \
    do {                                                                      \
        if (::osgGISPlugins::production::traceEnabled()) {                    \
            std::ostringstream _wb_trace_ss;                                  \
            _wb_trace_ss << msg;                                              \
            ::osgGISPlugins::production::traceLine(_wb_trace_ss.str());       \
        }                                                                     \
    } while (0)

#endif
