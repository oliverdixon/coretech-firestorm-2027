/**
 * @file
 *
 * TestHarness specification
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#ifndef FIRESTORM_TESTHARNESS_HPP
#define FIRESTORM_TESTHARNESS_HPP

#include <format>
#include <source_location>
#include <sstream>
#include <string>

namespace firestorm::test
{

class TestHarness final
{
public:
    struct TestCase
    {
        using TestFunction = void (*)(TestHarness&);

        std::string name;
        TestFunction function;
    };

    void check(bool condition, std::string_view expression, const std::source_location& location);

    template <typename Actual, typename Expected>
    void check_equals(const Actual& actual,
                      const Expected& expected,
                      const std::string_view actual_expression,
                      const std::string_view expected_expression,
                      const std::source_location& location)
    {
        ++assertion_count;

        if (actual == expected)
            return;

        const auto message = std::format("{} == {}\n\tActual: {}\n\tExpected: {}",
                                         actual_expression,
                                         expected_expression,
                                         represent(actual),
                                         represent(expected));

        report_failure(message, location);
    }

    template <typename ExpectedException, typename Function>
    void check_throws(Function&& function,
                      const std::string_view expression,
                      const std::string_view exception_name,
                      const std::source_location& location)
    {
        ++assertion_count;

        try {
            std::forward<Function>(function)(*this);
        } catch (const ExpectedException&) {
            return;
        } catch (const std::exception& actual) {
            const auto message = std::format("{} threw an unexpected exception.\n\tExpected {}\n\tActual: {}",
                                             expression,
                                             exception_name,
                                             actual.what());
            report_failure(message, location);
            return;
        } catch (...) {
            const auto message =
                std::format("{} threw a non-standard unexpected exception instead of {}.", expression, exception_name);
            report_failure(message, location);
            return;
        }

        const auto message = std::format("{} did not throw {}.", expression, exception_name);
        report_failure(message, location);
    }

    static int run(std::initializer_list<TestCase> test_cases);

private:
    TestHarness() = default;

    void report_failure(std::string_view message, const std::source_location& location);

    template <typename T> static std::string represent(const T& value)
    {
        if constexpr (requires(std::ostream& stream, const T& item) { stream << item; }) {
            std::ostringstream output;
            output << value;
            return std::move(output).str();
        }

        return "<value is not serialisable>";
    }

    std::size_t assertion_count = 0;
    std::size_t failure_count = 0;
};

} // namespace firestorm::test

#define CHECK(expression)                                                                                              \
    do {                                                                                                               \
        harness.check(static_cast<bool>(expression), #expression, std::source_location::current());                    \
    } while (false)

#define CHECK_EQUALS(actual, expected)                                                                                 \
    do {                                                                                                               \
        auto&& test_actual = actual;                                                                                   \
        auto&& test_expected = expected;                                                                               \
        harness.check_equals(test_actual, test_expected, #actual, #expected, std::source_location::current());         \
    } while (false)

#define CHECK_THROWS(expression, exception_type)                                                                       \
    do {                                                                                                               \
        harness.check_throws<exception_type>(                                                                          \
            [&]([[maybe_unused]] firestorm::test::TestHarness&) {                                                      \
                static_cast<void>(expression);                                                                         \
            },                                                                                                         \
            #expression,                                                                                               \
            #exception_type,                                                                                           \
            std::source_location::current());                                                                          \
    } while (false)

#endif // FIRESTORM_TESTHARNESS_HPP
