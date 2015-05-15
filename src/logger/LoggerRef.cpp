#include "LoggerRef.h"

using namespace Log;

LoggerRef::LoggerRef(ILogger& logger, const std::string& category) : logger(logger), category(category) {
}

LoggerRef::LoggerRef(const LoggerRef& other) : logger(other.logger), category(other.category) {
}

LoggerRef::LoggerRef(const LoggerRef& other, const std::string& category) : logger(other.logger), category(category) {
}

LoggerMessage LoggerRef::operator()(const std::string& category, ILogger::Level level, const std::string& color) {
  return LoggerMessage(logger, category, level, color);
}

LoggerMessage LoggerRef::operator()(ILogger::Level level, const std::string& color) {
  return LoggerMessage(logger, category, level, color);
}
