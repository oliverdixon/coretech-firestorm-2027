/**
 * @file
 *
 * TestDecoder specification
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#include "../../TrafficDecoder.hpp"
#include "../CTFSPacketStreamBuilder.hpp"
#include "../TestHarness.hpp"

using namespace firestorm;

namespace
{

void complete_packet_produces_one_record(test::TestHarness& harness)
{
    const std::string payload = "%PDF-test document\n%%EOF";
    const auto encoded_packet = CTFSPacketStreamBuilder::make_ctfs_record({
        .sequence_number = 0,
        .file_id = 1,
        .payload = std::as_bytes(std::span{payload}),
    });

    TrafficDecoder decoder;
    std::size_t record_count = 0;

    decoder.consume(encoded_packet, [&record_count](const TrafficRecordView&) { ++record_count; });
    decoder.finish();

    CHECK_EQUALS(record_count, 1);
}

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
        { "complete_packet_produces_one_record",complete_packet_produces_one_record },
        { "empty_input_is_accepted",empty_input_is_accepted },
        { "partial_magic_is_rejected_at_end_of_stream", partial_magic_is_rejected_at_end_of_stream }
    });

    // ReSharper restore CppUseDesignatedInitializers
    // clang-format on
}
