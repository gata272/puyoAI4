#include "ai/evaluation/debug_log.h"
#include <cassert>

int main() {
    puyo::setDebugLogging(true);
    puyo::debugLog("debug-log-test");
    auto s = puyo::takeDebugLog();
    assert(s.find("debug-log-test") != std::string::npos);

    puyo::setDebugLogging(false);
    puyo::debugLog("should-not-appear");
    assert(puyo::takeDebugLog().empty());
    return 0;
}
