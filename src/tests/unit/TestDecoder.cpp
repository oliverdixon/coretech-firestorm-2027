/**
 * @file
 *
 * TestDecoder specification
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#include "../../TrafficDecoder.hpp"
#include "../TestHarness.hpp"

using namespace firestorm;

namespace
{

void empty_input_is_accepted(test::TestHarness& harness)
{
    TrafficDecoder decoder;

    decoder.consume({}, [&](const TrafficRecordView&) {
        CHECK(false);
    });

    decoder.finish();
    CHECK(true);
}

void partial_magic_is_rejected_at_end_of_stream(test::TestHarness& harness)
{
    TrafficDecoder decoder;

    constexpr std::array input{
        std::byte{'C'},
        std::byte{'T'},
        std::byte{'F'},
        std::byte{'S'},
    };

    decoder.consume(input, [&harness](const TrafficRecordView&) {
        CHECK(false);
    });

    CHECK_THROWS(decoder.finish(), std::runtime_error);
}

} // namespace

int main()
{
    // clang-format off
    // ReSharper disable CppUseDesignatedInitializers

    return test::TestHarness::run({
        { "empty_input_is_accepted",empty_input_is_accepted },
        { "partial_magic_is_rejected_at_end_of_stream", partial_magic_is_rejected_at_end_of_stream }
    });

    // ReSharper restore CppUseDesignatedInitializers
    // clang-format on
}
