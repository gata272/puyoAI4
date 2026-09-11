#pragma once
#include <string>

namespace puyo {

void setDebugLogging(bool enabled);
bool debugLoggingEnabled();
void debugLog(const std::string& message);
std::string takeDebugLog();

} // namespace puyo
