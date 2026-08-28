/**
 * @file
 *
 * FireStorm implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#include <cstddef>
#include <exception>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

#include "../Driver.hpp"
#include "../Logger.hpp"

/**
 * FIRE STORM entry point.
 *
 * Runs FIRE STORM with its configured endpoints and output destination.
 *
 * Usage: `./FireStorm [--classified] [--drop-bad-checksums]`
 *
 * In classified mode, the application will follow the MAC-based routing rules. Otherwise, all non-operational data is
 * forwarded to the primary TCP endpoint. Disabled by default.
 *
 * If bad checksums are dropped, packets reporting invalid checksums will be ignored. Disabled by default.
 *
 * @param argc Number of arguments passed on the command line.
 * @param argv Argument vector.
 *
 * @return Zero on success; nonzero if networking, parsing, or output fails.
 */
int main(const int argc, const char* const* const argv)
{
    using namespace firestorm;

    // Process arguments.

    struct Options
    {
        bool drop_bad_checksums = false;
        bool classified = false;
    } options;

    const auto args =
        std::span{argv, static_cast<std::size_t>(argc)} | std::views::transform([](const char* const arg) {
            return std::string_view{arg};
        });

    for (const auto arg : args)
        if (arg == "--drop-bad-checksums")
            options.drop_bad_checksums = true;
        else if (arg == "--classified")
            options.classified = true;

    // Run the driver and report fatal errors.

    try {
        Driver driver(options.classified, options.drop_bad_checksums);
        driver.run();
        return 0;
    } catch (const std::exception& error) {
        LOG_F_WARNING("FIRE STORM failed: {}", error.what());
    }

    return 1;
}
