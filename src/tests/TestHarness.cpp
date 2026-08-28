/**
 * @file
 *
 * TestHarness implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#include "TestHarness.hpp"

#include <functional>
#include <iostream>
#include <ranges>
#include <regex>

namespace firestorm::test
{

void TestHarness::check(const bool condition, const std::string_view expression, const std::source_location& location)
{
    ++assertion_count;
    if (!condition)
        report_failure(expression, location);
}

int TestHarness::run(const int argc, const char* const* const argv, const std::initializer_list<TestCase> test_cases)
{
    TestHarness harness;

    const auto args =
        std::span{argv, static_cast<std::size_t>(argc)} | std::views::transform([](const char* const arg) noexcept {
            return std::string_view{arg};
        });

    bool run = false;
    std::function<bool(const TestCase&)> predicate;

    if (args.size() >= 3 && std::ranges::equal(args[1], std::string_view{"--test"})) {
        predicate = [&run, match = args[2]](const TestCase& test_case) {
            const auto matches = std::ranges::equal(test_case.id, match);
            if (matches)
                run = true;
            return matches;
        };
    } else {
        std::cout << "No test specification given; running all tests in fixture." << std::endl;
        predicate = []([[maybe_unused]] const TestCase&) noexcept {
            return true;
        };
    }

    for (const auto& [id, name, function] : test_cases | std::views::filter(predicate)) {
        std::println(std::cout, "[ RUN    ] {} ({})", id, name);
        const auto failures_before = harness.failure_count;

        try {
            function(harness);
        } catch (const std::exception& exception) {
            harness.report_failure(std::string{"Unhandled exception: "} + exception.what(),
                                   std::source_location::current());
        } catch (...) {
            harness.report_failure("Unhandled non-standard exception.", std::source_location::current());
        }

        if (harness.failure_count == failures_before)
            std::println(std::cout, "[ OK     ] {}", id);
        else
            std::println(std::cerr, "[ FAILED ] {}", id);
    }

    if (!run) {
        std::cerr << "No test cases were run." << std::endl;
        return EXIT_FAILURE;
    }

    return harness.failure_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

void TestHarness::report_failure(std::string_view message, const std::source_location& location)
{
    ++failure_count;
    std::println(std::cerr, "{}:{}: CHECK FAILED: {}", location.file_name(), location.line(), message);
}

} // namespace firestorm::test
