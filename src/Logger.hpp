/**
 * @file
 * @brief Logger specification
 * @author Oliver Dixon
 * @date 2026-08-21
 */

#ifndef FIRESTORM_LOGGER_HPP
#define FIRESTORM_LOGGER_HPP

#include <format>
#include <source_location>
#include <string_view>

namespace firestorm
{

/**
 * Provides a simple statically accessible console logging interface.
 */
class Logger
{
public:
    /**
     * Indicates the level of importance of a logged message.
     */
    enum class Level
    {
        Info,
        Warning
    };

    /**
     * Log an unformatted message to the backend.
     *
     * @param level The level of the message.
     * @param message The unformatted message (string literal).
     * @param location The optional origin of the message.
     *
     * @note If @c NDEBUG is set, this function is a no-op.
     */
    static void log(Level level,
                    std::string_view message,
                    std::source_location location = std::source_location::current());

    /**
     * Log a formatted message to the backend with a specific source location.
     *
     * @tparam Args Types of variadic arguments for the formatter.
     * @param level The level of the message.
     * @param location The origin of the message.
     * @param fmt The @c printf style format string of the message.
     * @param args The values for the format string.
     *
     * @note If @c NDEBUG is set, this function is a no-op.
     */
    template <typename... Args>
    static void log_f([[maybe_unused]] const Level level,
                      [[maybe_unused]] const std::source_location location,
                      [[maybe_unused]] std::format_string<Args...> fmt,
                      [[maybe_unused]] Args&&... args)
    {
#ifndef NDEBUG
        log(level, std::format(fmt, std::forward<Args>(args)...), location);
#endif
    }

private:
    /**
     * Get a human-readable string corresponding to the given log level.
     *
     * @param level Relevant log level.
     * @return Human-readable string for the log level.
     */
    static std::string_view level_to_string(Level level);
};

} // namespace firestorm

#ifndef NDEBUG

/**
 * Logs a formatted info-level message using firestorm::Logger::log_f.
 *
 * @param msg The format string, followed by formatted values.
 */
#define LOG_F_INFO(msg, ...)                                                                                           \
    do {                                                                                                               \
        Logger::log_f(Logger::Level::Info, std::source_location::current(), msg, __VA_ARGS__);                         \
    } while (0)

/**
 * Logs a formatted warning-level message using firestorm::Logger::log_f.
 *
 * @param msg The format string, followed by formatted values.
 */
#define LOG_F_WARNING(msg, ...)                                                                                        \
    do {                                                                                                               \
        Logger::log_f(Logger::Level::Warning, std::source_location::current(), msg, __VA_ARGS__);                      \
    } while (0)

/**
 * Logs an unformatted info-level message using firestorm::Logger::log.
 *
 * @param msg The string literal to log.
 */
#define LOG_INFO(msg)                                                                                                  \
    do {                                                                                                               \
        Logger::log(Logger::Level::Info, msg, std::source_location::current());                                        \
    } while (0)

/**
 * Logs an unformatted warning-level message using firestorm::Logger::log.
 *
 * @param msg The string literal to log.
 */
#define LOG_WARNING(msg)                                                                                               \
    do {                                                                                                               \
        Logger::log(Logger::Level::Warning, msg, std::source_location::current());                                     \
    } while (0)

#else

#define LOG_F_INFO(msg, ...)
#define LOG_F_WARNING(msg, ...)
#define LOG_INFO(msg)
#define LOG_WARNING(msg)

#endif

#endif // FIRESTORM_LOGGER_HPP
