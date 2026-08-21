/**
 * @file
 * @brief Logger implementation
 * @author Oliver Dixon
 * @date 2026-06-20
 */

#include "Logger.hpp"

#include <iostream>
#include <syncstream>

namespace firestorm
{

void Logger::log(const Level level, const std::string_view message, const std::source_location location)
{
    const auto line =
        std::format("[{}] {}:{} says: {}", level_to_string(level), location.file_name(), location.line(), message);

    auto stream = std::osyncstream(level == Level::Error ? std::cerr : std::cout);
    stream << line << '\n';
}

std::string_view Logger::level_to_string(const Level level)
{
    switch (level) {
    case Level::Info:
        return "Info";
    case Level::Error:
        return "Error";
    }

    return "Unknown";
}

} // namespace firestorm
