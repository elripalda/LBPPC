// bbport: see log.h.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include "common/logging/log.h"

namespace Common::Log {
static Level threshold() {
    static const Level level = [] {
        const char* v = std::getenv("BB_GPU_LOG");
        if (!v) return Level::warn;
        static const char* names[] = {"trace", "debug", "info", "warning", "error", "critical"};
        for (int i = 0; i < 6; ++i)
            if (!std::strcmp(v, names[i])) return Level(i);
        return Level::warn;
    }();
    return level;
}
static bool OnlineClass(std::string_view log_class) {
    static const bool enabled = [] {
        const char* v = std::getenv("BB_ONLINE_LOG");
        return v && v[0] == '1';
    }();
    return enabled && (log_class.starts_with("Lib.Np") || log_class.starts_with("Lib.Http") ||
                       log_class == "NpHandler" || log_class == "ShadNet");
}
bool ShouldLog(std::string_view log_class, Level level) {
    return level >= threshold() || (level >= Level::info && OnlineClass(log_class));
}
void Write(std::string_view log_class, Level level, const char* file, int line, const char* function,
           std::string&& message) {
    static std::mutex mutex;
    static const char* names[] = {"Trace", "Debug", "Info", "Warning", "Error", "Critical"};
    const char* base = std::strrchr(file, '/');
    std::scoped_lock lock{mutex};
    std::fprintf(stderr, "GPU [%.*s] <%s> %s:%d %s: %s\n", int(log_class.size()), log_class.data(),
                 names[int(level)], base ? base + 1 : file, line, function, message.c_str());
}
} // namespace Common::Log
