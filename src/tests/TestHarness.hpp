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
#include <vector>

namespace firestorm::test
{

/**
 * Runs test cases and records non-fatal assertion failures.
 *
 * TestHarness provides a small dependency-free testing interface for the FIRE STORM unit tests. Each test case receives
 * a reference to the active harness and uses the CHECK macros to record assertions.
 *
 * A failed assertion is reported immediately but not does not terminate the current case. Exceptions that escape a test
 * case are caught by @ref run and reported as failures.
 *
 * @note TestHarness is not thread-safe. Assertions should be performed by the thread executing the case.
 * @note The CHECK macros require a TestHarness variable names @c harness to be in scope.
 */
class TestHarness final
{
public:
    /**
     * Describes one named test case.
     */
    struct TestCase
    {
        /**
         * Function signature used to execute a test case.
         *
         * The supplied TestHarness should be used to record all assertions performed by the test.
         */
        using TestFunction = void (*)(TestHarness&);

        std::string id;        /**< Internal stable test name. */
        std::string name;      /**< Human-readable test name. */
        TestFunction function; /**< Function implementing the test case. */
    };

    /**
     * Verifies the result of a boolean assertion.
     *
     * The assertion count is incremented on every invocation. If @p condition is false, a diagnostic containing @p
     * expression and @p location is issued and the failure count is incremented.
     *
     * A failed check is non-fatal; execution of the test continues.
     *
     * @param condition Evaluated result of the asserted expression.
     * @param expression Textual representation of @p condition.
     * @param location Source location at which the assertion was made.
     */
    void check(bool condition, std::string_view expression, const std::source_location& location);

    /**
     * Verifies that two values compare equal.
     *
     * Both values are evaluated by the calling macro before this function is entered. If the values do not compare
     * equal, their textual representations are included in the failure diagnostic.
     *
     * @tparam Actual Type of the actual value.
     * @tparam Expected Type of the expected value.
     *
     * @param actual Actual value produced by the code under test.
     * @param expected Expected value against which @p actual is compared.
     * @param actual_expression Textual representation of @p actual.
     * @param expected_expression Textual representation of @p expected.
     * @param location Source location at which the assertion was made.
     */
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
                                         try_serialise(actual),
                                         try_serialise(expected));

        report_failure(message, location);
    }

    /**
     * Verifies that invoking a function throws a specified exception type.
     *
     * The supplied function is invoked exactly once with a reference to this TestHarness. The assertion succeeds if it
     * throws @p ExpectedException or a type derived from it. The assertion fails if the function:
     *
     *  - returns normally (does not throw);
     *  - throws a different standard exception; or
     *  - throws a non-standard exception.
     *
     * @tparam ExpectedException Expected exception type.
     * @tparam Function Callable type accepting a TestHarness reference.
     *
     * @param function Function to invoke.
     * @param expression Textual representation of @p function.
     * @param exception_name Textual representation of @p ExpectedException.
     * @param location Source location at which the assertion was made.
     */
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

    /**
     * Executes a suite of test cases.
     *
     * If the "--test" command-line parameter is omitted, test cases are executed sequentially in the order supplied.
     * If "--test" is included, followed by a single test ID, only the test with the matching ID is executed. For ease
     * of use, the @p argc and @p argv can be passed directly as provided by the shell; that is, @c argv[0] is expected
     * to contain the program name.
     *
     * An exception escaping a test function is converted into a failed assertion, after which execution continues with
     * the next test case.
     *
     * @param argc Number of arguments, passed from the shell.
     * @param argv Argument vector, passed from the shell.
     * @param test_cases Test cases to execute.
     *
     * @return @c EXIT_SUCCESS if every test case passes; @c EXIT_FAILURE otherwise, or if no test cases were selected.
     */
    static int run(int argc, const char* const* argv, std::initializer_list<TestCase> test_cases);

    /**
     * Concatenate ranges into a single vector of bytes.
     *
     * This is a shim for pre-C++26 standard libraries that do not provide @c std::views::concatenate.
     *
     * @tparam Buffers Types of constituent ranges
     * @param buffers Constituent ranges
     *
     * @return Concatenated @p buffers
     */
    template <typename... Buffers>
    static std::vector<std::byte> concatenate(const Buffers&... buffers)
    {
        std::vector<std::byte> result;
        result.reserve((std::size_t{0} + ... + buffers.size()));
        (result.insert(result.end(), buffers.begin(), buffers.end()), ...);
        return result;
    }

private:
    /**
     * Constructs an empty harness.
     *
     * Construction is private because harness lifetime is managed by @ref run.
     */
    TestHarness() = default;

    /**
     * Records and prints an assertion failure.
     *
     * @param message Human-readable description of the failure.
     * @param location Source location associated with the assertion failure.
     */
    void report_failure(std::string_view message, const std::source_location& location);

    /**
     * Produces a human-readable representation of a value.
     *
     * If @p T supports insertion into an output string stream, its stream representation is returned. Otherwise, a
     * placeholder is returned.
     *
     * @tparam T Type of value to serialise.
     * @param value Value to serialise.
     *
     * @return Human-readable representation of @p value.
     */
    template <typename T>
    static std::string try_serialise(const T& value)
    {
        if constexpr (requires(std::ostream& stream, const T& item) { stream << item; }) {
            std::ostringstream output;
            output << value;
            return std::move(output).str();
        }

        return "<value is not serialisable>";
    }

    std::size_t assertion_count = 0; /**< Number of total assertions evaluated. */
    std::size_t failure_count = 0;   /**< Number of failed assertions. */
};

} // namespace firestorm::test

/**
 * @def CHECK
 *
 * Records whether a Boolean expression evaluates as true.
 *
 * @param expression Expression convertible to a boolean.
 *
 * @see firestorm::test::TestHarness::check
 * @pre A firestorm::test::TestHarness variable named @c harness is in scope.
 */
#define CHECK(expression)                                                                                              \
    do {                                                                                                               \
        harness.check(static_cast<bool>(expression), #expression, std::source_location::current());                    \
    } while (false)

/**
 * @def CHECK_EQUALS
 *
 * Records whether two expressions compare equal.
 *
 * @param actual Expression producing the actual value.
 * @param expected Expression producing the expected value.
 *
 * @see firestorm::test::TestHarness::check_equals
 * @pre A firestorm::test::TestHarness variable named @c harness is in scope.
 */
#define CHECK_EQUALS(actual, expected)                                                                                 \
    do {                                                                                                               \
        auto&& test_actual = actual;                                                                                   \
        auto&& test_expected = expected;                                                                               \
        harness.check_equals(test_actual, test_expected, #actual, #expected, std::source_location::current());         \
    } while (false)

/**
 * @def CHECK_THROWS
 *
 * Records whether an expression throws a specified exception type.
 *
 * @param expression Expression expected to throw.
 * @param exception_type Expected exception type.
 *
 * @see firestorm::test::TestHarness::check_throws
 * @pre A firestorm::test::TestHarness variable named @c harness is in scope.
 */
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
