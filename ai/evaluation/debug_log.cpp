#include "debug_log.h"
#include <iostream>
#include <mutex>
#include <sstream>

namespace puyo {
namespace {
bool g_enabled = false;
std::ostringstream g_log;
std::mutex g_mutex;
}

void setDebugLogging(bool enabled) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_enabled = enabled;
    if (!enabled) g_log.str(""), g_log.clear();
}

bool debugLoggingEnabled() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_enabled;
}

void debugLog(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_enabled) return;
    std::cout << message << std::endl;
    g_log << message << '\n';
}

std::string takeDebugLog() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::string result = g_log.str();
    g_log.str("");
    g_log.clear();
    return result;
}
} // namespace puyo
