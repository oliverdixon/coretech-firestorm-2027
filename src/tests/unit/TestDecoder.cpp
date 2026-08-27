/**
 * @file
 *
 * TestDecoder specification
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#include <ranges>

#include "../../TrafficDecoder.hpp"
#include "../CTFSPacketStreamBuilder.hpp"
#include "../TestHarness.hpp"

using namespace firestorm;
using namespace firestorm::test;

namespace
{

void decode_valid_packet(TestHarness& harness)
{
    const std::string payload = "%PDF-test document\n%%EOF";
    const auto encoded_packet = CTFSPacketStreamBuilder::make_ctfs_record({
        .sequence_number = 0x1234,
        .file_id = 0xEE,
        .payload = std::as_bytes(std::span{payload}),
    });

    TrafficDecoder decoder;
    std::size_t record_count = 0;

    decoder.consume(encoded_packet, [&harness, &payload, &record_count](const TrafficRecord& record) {
        ++record_count;

        CHECK(std::holds_alternative<CTFSPacket>(record));

        if (const auto* packet = std::get_if<CTFSPacket>(&record); packet != nullptr) {
            CHECK_EQUALS(packet->get_sequence_number(), 0x1234);
            CHECK_EQUALS(packet->get_file_id(), 0xEE);
            CHECK(std::ranges::equal(packet->observe_payload(), std::as_bytes(std::span{payload})));
        }
    });

    decoder.finish();
    CHECK_EQUALS(record_count, 1);
}

void accept_empty_input(test::TestHarness& harness)
{
    TrafficDecoder decoder;

    decoder.consume({}, [&](const TrafficRecord&) {
        CHECK(false);
    });

    decoder.finish();
    CHECK(true);
}

void reject_partial_header(TestHarness& harness)
{
    TrafficDecoder decoder;

    constexpr std::array input{
        std::byte{'C'},
        std::byte{'T'},
        std::byte{'F'},
        std::byte{'S'},
    };

    decoder.consume(input, [&harness](TrafficRecord&&) {
        CHECK(false);
    });

    CHECK_THROWS(decoder.finish(), std::runtime_error);
}

void reject_bad_checksum(TestHarness& harness)
{
    const std::string payload = "%PDF-test document\n%%EOF";
    const auto encoded_packet = CTFSPacketStreamBuilder::make_ctfs_record({
        .sequence_number = 0,
        .file_id = 1,
        .payload = std::as_bytes(std::span{payload}),
        .checksum = 0xDEADBEEF
    });

    TrafficDecoder decoder(true);
    CHECK_THROWS(decoder.consume(encoded_packet,
                                 [&](TrafficRecord&&) {
                                 }),
                 std::runtime_error);
}

void decode_segmented_packets(TestHarness& harness)
{
    const std::string payload = "%PDF-test document\n%%EOF";
    const auto encoded_packet = CTFSPacketStreamBuilder::make_ctfs_record({
        .sequence_number = 0x4321,
        .file_id = 0xAA,
        .payload = std::as_bytes(std::span{payload}),
    });

    const std::span bytes{encoded_packet};

    for (std::size_t split = 0; split <= bytes.size(); ++split) {
        TrafficDecoder decoder;
        std::size_t callback_count = 0;
        std::optional<CTFSPacket> decoded_packet;

        const auto callback = [&harness, &callback_count, &payload](TrafficRecord&& record) {
            ++callback_count;

            CHECK(std::holds_alternative<CTFSPacket>(record));

            if (const auto* packet = std::get_if<CTFSPacket>(&record); packet != nullptr) {
                CHECK_EQUALS(packet->get_sequence_number(), 0x4321);
                CHECK_EQUALS(packet->get_file_id(), 0xAA);
                CHECK(std::ranges::equal(packet->observe_payload(), std::as_bytes(std::span{payload})));
            }
        };

        decoder.consume(bytes.first(split), callback);
        decoder.consume(bytes.subspan(split), callback);
        decoder.finish();

        CHECK_EQUALS(callback_count, 1U);
    }
}

void reject_nonempty_truncated_packets(TestHarness& harness)
{
    const std::string payload = "%PDF-test document\n%%EOF";
    const auto encoded_packet = CTFSPacketStreamBuilder::make_ctfs_record({
        .payload = std::as_bytes(std::span{payload}),
    });

    const std::span bytes{encoded_packet};

    for (std::size_t length = 1; length < bytes.size(); ++length) {
        TrafficDecoder decoder;
        std::size_t callback_count = 0;

        decoder.consume(bytes.first(length), [&callback_count](TrafficRecord&&) {
            ++callback_count;
        });

        CHECK_EQUALS(callback_count, 0U);
        CHECK_THROWS(decoder.finish(), std::runtime_error);
    }
}

} // namespace

int main()
{
    // clang-format off
    // ReSharper disable CppUseDesignatedInitializers

    return TestHarness::run({
        { "Complete packet produces exactly one valid record",decode_valid_packet },
        { "Empty input is accepted",accept_empty_input },
        { "Partial header is rejected as end-of-stream", reject_partial_header },
        { "Invalid checksum is rejected", reject_bad_checksum },
        { "Packet decodes correctly independent of segmentation", decode_segmented_packets },
        { "Truncates packets are rejected", reject_nonempty_truncated_packets }
    });

    // ReSharper restore CppUseDesignatedInitializers
    // clang-format on
}
