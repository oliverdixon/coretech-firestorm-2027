/**
 * @file
 *
 * TestHarness implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#include "TestHarness.hpp"

#include <iostream>

namespace firestorm::test
{

void TestHarness::check(const bool condition, const std::string_view expression, const std::source_location& location)
{
    ++assertion_count;
    if (!condition)
        report_failure(expression, location);
}

int TestHarness::run(const std::initializer_list<TestCase> test_cases)
{
    TestHarness harness;

    for (const auto& [name, function] : test_cases) {
        std::println(std::cout, "[ RUN    ] {}", name);
        const auto failures_before = harness.failure_count;

        try {
            function(harness);
        } catch (const std::exception& exception) {
            harness.report_failure(std::string{"Unhandled exception: "} + exception.what(), std::source_location::current());
        } catch (...) {
            harness.report_failure("Unhandled non-standard exception.", std::source_location::current());
        }

        if (harness.failure_count == failures_before)
            std::println(std::cout, "[ OK     ] {}", name);
        else
            std::println(std::cout, "[ FAILED ] {}", name);
    }

    return harness.failure_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

void TestHarness::report_failure(std::string_view message, const std::source_location& location)
{
    ++failure_count;
    std::println(std::cerr, "{}:{}: CHECK FAILED: {}", location.file_name(), location.line(), message);
}

} // namespace firestorm::test
