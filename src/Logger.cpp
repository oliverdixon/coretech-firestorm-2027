/**
 * @file
 * @brief Logger implementation
 * @author Oliver Dixon
 * @date 2026-06-20
 */

#include "Logger.hpp"

#include <iostream>
#include <syncstream>
#include <utility>

namespace firestorm
{

void Logger::log([[maybe_unused]] const Level level,
                 [[maybe_unused]] const std::string_view message,
                 [[maybe_unused]] const std::source_location location)
{
#ifndef NDEBUG
    const auto line =
        std::format("[{}] {}:{} says: {}", level_to_string(level), location.file_name(), location.line(), message);

    auto stream = std::osyncstream(level == Level::Warning ? std::cerr : std::cout);
    stream << line << '\n';
#endif
}

std::string_view Logger::level_to_string(const Level level)
{
    switch (level) {
    case Level::Info:
        return "Info";
    case Level::Warning:
        return "Warn";
    }

    std::unreachable();
}

} // namespace firestorm
