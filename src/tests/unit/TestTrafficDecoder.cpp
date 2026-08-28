/**
 * @file
 *
 * TestTrafficDecoder specification
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

/**
 * Validate a TrafficRecord and append to a destination vector.
 *
 * @param harness The active TestHarness.
 * @param output The destination for @p record.
 * @param record The TrafficRecord validated to hold the CTFSPacket alternative.
 */
void capture_ctfs_packet(TestHarness& harness, std::vector<CTFSPacket>& output, TrafficRecord&& record)
{
    CHECK(std::holds_alternative<CTFSPacket>(record));
    auto* const packet = std::get_if<CTFSPacket>(&record);
    if (packet != nullptr)
        output.emplace_back(std::move(*packet));
}

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
template <typename... Buffers> std::vector<std::byte> concatenate(const Buffers&... buffers)
{
    std::vector<std::byte> result;
    result.reserve((std::size_t{0} + ... + buffers.size()));
    (result.insert(result.end(), buffers.begin(), buffers.end()), ...);
    return result;
}

/**
 * @test Verifies that a single compliant CTFSPacket is correctly decoded by the TrafficDecoder.
 *
 * @param harness Active harness.
 */
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

/**
 * @test Verifies that empty input is accepted by the TrafficDecoder.
 *
 * The consumption callback must not be invoked.
 *
 * @param harness Active harness.
 */
void accept_empty_input(test::TestHarness& harness)
{
    TrafficDecoder decoder;

    decoder.consume({}, [&](const TrafficRecord&) {
        CHECK(false);
    });

    decoder.finish();
    CHECK(true);
}

/**
 * @test Verifies that a partial CTFS header is rejected by the TrafficDecoder.
 *
 * In particular, the bytes "CTFS" without any following header or payload is rejected. The consumption callback must
 * not be invoked.
 *
 * @param harness Active harness.
 */
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

/**
 * @test Verifies that a CTFS packet with an invalid checksum is rejected by the TrafficDecoder.
 *
 * @param harness Active harness.
 */
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

/**
 * @test Verifies that valid CTFS packets are decoded correctly across any segmentation boundary.
 *
 * This test verifies that, regardless of split (of which all possible ones are tested), the TrafficDecoder correctly
 * buffers and recovers data across consumption call boundaries and does not prematurely invoke the callback.
 *
 * @param harness Active harness.
 */
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

/**
 * @test Verifies that truncated CTFS packets are rejected by the TrafficDecoder.
 *
 * Despite a valid stream, the TrafficDecoder must reject an incomplete packet, determined by its advertised length.
 *
 * @param harness Active harness.
 */
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

/**
 * @test Verifies that concatenated records sent in a single stream are decoded by the TrafficDecoder.
 *
 * @param harness Active harness.
 */
void decode_coalesced_records(TestHarness& harness)
{
    const std::string first_pl = "first";
    const std::string second_pl = "second";
    const std::string third_pl = "third";

    const auto first = CTFSPacketStreamBuilder::make_ctfs_record(
        {.sequence_number = 0, .payload = std::as_bytes(std::span{first_pl})});

    const auto second = CTFSPacketStreamBuilder::make_ctfs_record(
        {.sequence_number = 1, .payload = std::as_bytes(std::span{second_pl})});

    const auto third = CTFSPacketStreamBuilder::make_ctfs_record(
        {.sequence_number = 2, .payload = std::as_bytes(std::span{third_pl})});

    const auto stream = concatenate(first, second, third);

    TrafficDecoder decoder;
    std::vector<CTFSPacket> decoded;

    decoder.consume(stream, [&harness, &decoded](TrafficRecord&& record) {
        capture_ctfs_packet(harness, decoded, std::move(record));
    });

    decoder.finish();

    CHECK_EQUALS(decoded.size(), 3);
}

/**
 * @test Verifies that CTFS packets with padding bytes set to non-zero are rejected.
 *
 * @param harness Active harness.
 */
void reject_nonzero_padding(TestHarness& harness)
{
    const std::string payload = "pl_upward";
    const auto encoded = CTFSPacketStreamBuilder::make_ctfs_record(
        {.payload = std::as_bytes(std::span{payload}), .padding = {std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}}});

    TrafficDecoder decoder;
    CHECK_THROWS(decoder.consume(encoded,
                                 [](TrafficRecord&&) {
                                 }),
                 std::runtime_error);
}

/**
 * @test Verifies that CTFS packets with an incorrect advertised length are rejected.
 *
 * @param harness Active harness.
 */
void reject_incorrect_advertised_length(TestHarness& harness)
{
    const std::string payload = "pl_barnblitz";
    const auto encoded = CTFSPacketStreamBuilder::make_ctfs_record(
        {.payload = std::as_bytes(std::span{payload}),
         .advertised_length = static_cast<std::uint16_t>(payload.size() + 0x0F)});

    TrafficDecoder decoder;

    decoder.consume(encoded, [&harness](TrafficRecord&&) {
        CHECK(false);
    });

    CHECK_THROWS(decoder.finish(), std::runtime_error);
}

/**
 * @test Verifies that the TrafficDecoder can be reused.
 *
 * @param harness Active harness.
 */
void decoder_can_be_reused(TestHarness& harness)
{
    const std::string first_pl = "first";
    const std::string second_pl = "second";

    const auto first = CTFSPacketStreamBuilder::make_ctfs_record(
        {.sequence_number = 0, .payload = std::as_bytes(std::span{first_pl})});

    const auto second = CTFSPacketStreamBuilder::make_ctfs_record(
        {.sequence_number = 1, .payload = std::as_bytes(std::span{second_pl})});

    TrafficDecoder decoder;
    std::vector<CTFSPacket> decoded;
    const auto callback = [&harness, &decoded](TrafficRecord&& record) {
        capture_ctfs_packet(harness, decoded, std::move(record));
    };

    decoder.consume(first, callback);
    decoder.finish();

    decoder.consume(second, callback);
    decoder.finish();

    CHECK_EQUALS(decoded.size(), 2U);
}

/**
 * @test Verifies that the TrafficDecoder can recover from an invalid packet.
 *
 * A single TrafficDecoder must not enter an undefined or unusable state following a bad path. Following an
 * error/exception, it must discard the involved bytes and be capable of continuing to consume data and produce
 * TrafficRecord objects.
 *
 * @param harness Active harness.
 */
void decoder_can_recover(TestHarness& harness)
{
    const std::string bad_pl = "bad";
    const std::string good_pl = "good";

    const auto bad = CTFSPacketStreamBuilder::make_ctfs_record(
        {.sequence_number = 0xAA,
         .file_id = 0xBB,
         .payload = std::as_bytes(std::span{bad_pl}),
         .checksum = 0xDEADBEEF});

    const auto good = CTFSPacketStreamBuilder::make_ctfs_record(
        {.sequence_number = 0xCC, .file_id = 0xDD, .payload = std::as_bytes(std::span{good_pl})});

    const auto stream = concatenate(bad, good);

    TrafficDecoder decoder;
    std::vector<CTFSPacket> decoded;
    const auto callback = [&harness, &decoded](TrafficRecord&& record) {
        capture_ctfs_packet(harness, decoded, std::move(record));
    };

    CHECK_THROWS(decoder.consume(stream, callback), std::runtime_error);
    CHECK(decoded.empty());

    decoder.consume({}, callback);
    decoder.finish();

    CHECK_EQUALS(decoded.size(), 1U);

    if (!decoded.empty()) {
        const auto& packet = decoded[0];
        CHECK_EQUALS(packet.get_sequence_number(), 0xCC);
        CHECK_EQUALS(packet.get_file_id(), 0xDD);
        CHECK(std::ranges::equal(packet.observe_payload(), std::as_bytes(std::span{good_pl})));
    }
}

/**
 * @test Verifies that the TrafficDecoder compaction works across buffer boundaries.
 *
 * @param harness Active harness.
 */
void compaction_preserves_partial(TestHarness& harness)
{
    std::vector large_pl(std::numeric_limits<std::uint16_t>::max(), std::byte{0xAA});
    std::string final_pl = "pl_badwater";

    const auto large = CTFSPacketStreamBuilder::make_ctfs_record({.sequence_number = 0, .payload = large_pl});

    const auto final = CTFSPacketStreamBuilder::make_ctfs_record(
        {.sequence_number = 1, .payload = std::as_bytes(std::span{final_pl})});

    const auto stream = concatenate(large, final);
    const auto split_pos = large.size() + 4;

    TrafficDecoder decoder;
    std::vector<CTFSPacket> decoded;
    const auto callback = [&harness, &decoded](TrafficRecord&& record) {
        capture_ctfs_packet(harness, decoded, std::move(record));
    };

    decoder.consume(std::span{stream}.first(split_pos), callback);
    CHECK_EQUALS(decoded.size(), 1U);

    decoder.consume(std::span{stream}.subspan(split_pos), callback);
    decoder.finish();
    CHECK_EQUALS(decoded.size(), 2U);

    if (decoded.size() == 2U) {
        CHECK_EQUALS(decoded[0].get_sequence_number(), 0);
        CHECK_EQUALS(decoded[1].get_sequence_number(), 1);

        CHECK(std::ranges::equal(decoded[1].observe_payload(), std::as_bytes(std::span{final_pl})));
    }
}

} // namespace

/**
 * Executes selected TrafficDecoder tests with a TestHarness.
 *
 * For the meanings of @p argc and @p argv, see TestHarness::run documentation.
 *
 * @param argc Argument count
 * @param argv Argument vector
 *
 * @return Zero if all executed tests passed; non-zero otherwise.
 */
int main(const int argc, const char* const* const argv)
{
    // clang-format off

    return TestHarness::run(argc, argv, {
        {
            .id = "decode_valid_packet",
            .name = "Complete packet produces exactly one valid record",
            .function = decode_valid_packet
        },
        {
            .id = "accept_empty_input",
            .name = "Empty input is accepted",
            .function = accept_empty_input
        },
        {
            .id = "reject_partial_header",
            .name = "Partial header is rejected as end-of-stream",
            .function = reject_partial_header
        },
        {
            .id = "reject_bad_checksum",
            .name = "Invalid checksum is rejected",
            .function = reject_bad_checksum
        },
        {
            .id = "decode_segmented_packets",
            .name = "Packet decodes correctly independent of segmentation",
            .function = decode_segmented_packets
        },
        {
            .id = "reject_nonempty_truncated_packets",
            .name = "Truncated packets are rejected",
            .function = reject_nonempty_truncated_packets
        },
        {
            .id = "decode_coalesced_records",
            .name = "Coalesced packets are decoded",
            .function = decode_coalesced_records
        },
        {
            .id = "reject_nonzero_padding",
            .name = "Packets with non-zero padding are rejected",
            .function = reject_nonzero_padding
        },
        {
            .id = "reject_incorrect_advertised_length",
            .name = "Incorrect advertised lengths are rejected",
            .function = reject_incorrect_advertised_length
        },
        {
            .id = "decoder_can_be_reused",
            .name = "Decoder can be reused",
            .function = decoder_can_be_reused
        },
        {
            .id = "decoder_can_recover",
            .name = "Decoder can recover from bad packets",
            .function = decoder_can_recover
        },
        {
            .id = "compaction_preserves_partial",
            .name = "Compaction preserves partial following record",
            .function = compaction_preserves_partial
        }
    });

    // clang-format on
}
