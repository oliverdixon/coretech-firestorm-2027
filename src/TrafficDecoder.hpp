/**
 * @file
 *
 * TrafficDecoder specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_TRAFFICDECODER_HPP
#define FIRESTORM_TRAFFICDECODER_HPP

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <variant>

#include "CTFSPacket.hpp"
#include "CTFSRecordView.hpp"
#include "EthernetFrameView.hpp"

namespace firestorm
{

/**
 * A non-owning decoded view of operational or non-operational data.
 */
using TrafficRecordView = std::variant<CTFSPacketView, EthernetFrameView>;

/**
 * Concept for callbacks that consume TrafficRecordView results.
 *
 * @tparam F Callable type accepting TrafficRecordView.
 */
template <typename F>
concept TrafficDecoderCallback = std::invocable<F&, const TrafficRecordView&>;

/**
 * Incrementally decodes typed traffic records from a byte stream.
 *
 * TrafficDecoder accepts arbitrary chunks of bytes that needn't align with record boundaries. It buffers incomplete
 * input and reports each complete record to a TrafficDecoderCallback as a TrafficRecordView.
 *
 * The input stream may contain encoded variants of the following record types:
 *  - CTFS packets, identified by a leader @c CTFS magic value; or
 *  - Ethernet frames containing arbitrary IPv4 packets.
 *
 * The interfaces are defined in /README.md.
 *
 * @note Ethernet record boundaries are determined from the IPv4 total-length field. The decoder currently only accepts
 * Ethernet frames carrying IPv4 packets.
 *
 * @note Views passed to a callback refer to internal storage and remain valid only for the duration of the callback
 * invocation. A callback must copy any data that it needs to retain.
 *
 * @warning TrafficDecoder is neither thread-safe nor reentrant.
 */
class TrafficDecoder
{
public:
    /**
     * Processes a chunk of input bytes.
     *
     * @param input Chunk of bytes from the input stream.
     * @param callback Callback invoked synchronously for ordinary bytes.
     */
    void consume(std::span<const std::byte> input, TrafficDecoderCallback auto&& callback);

    /**
     * Finishes processing after the input stream closes.
     *
     * Any retained bytes that cannot form a complete CTFS frame are forwarded as ordinary data through @p callback.
     *
     * @param callback Callback invoked synchronously for remaining ordinary bytes.
     *
     * @throws std::runtime_error The stream ends during a CTFS frame.
     *
     * @post On success, the filter contains no buffered input and may be reused for another stream.
     */
    void finish(TrafficDecoderCallback auto&& callback);

private:
    /**
     * CTFS frame marker.
     */
    static constexpr std::array magic{
        std::byte{0x43}, // 'C',
        std::byte{0x54}, // 'T',
        std::byte{0x46}, // 'F'
        std::byte{0x53}, // 'S'
    };

    /**
     * Decodes an unsigned 16-bit big-endian value.
     *
     * @param bytes Bytes containing the encoded value.
     *
     * @return Decoded value.
     */
    static std::uint16_t decode_big_endian(std::span<const std::byte, 2> bytes) noexcept;

    /**
     * Decodes an unsigned 32-bit big-endian value.
     *
     * @param bytes Bytes containing the encoded value.
     *
     * @return Decoded value.
     */
    static std::uint32_t decode_big_endian(std::span<const std::byte, 4> bytes) noexcept;

    /**
     * Attempts to decode one CTFS packet from the beginning of @p available.
     *
     * If a complete packet is available, constructs a CTFSPacketView, and invokes @p callback with the view. The
     * read head is advanced past the packet. If the header or payload is incomplete, no callback is invoked and no
     * bytes are consumed.
     *
     * @param available Unprocessed bytes beginning with a CTFS packet.
     * @param callback Callback to invoke with the decoded CTFSPacketView.
     *
     * @return True if a complete CTFS packet was consumed; false otherwise.
     *
     * @note The CTFSPacketView supplied to @p callback refers to decoder-owned storage and remains valid only for the
     * duration of the callback invocation.
     */
    bool consume_ctfs_record(std::span<const std::byte> available, TrafficDecoderCallback auto&& callback);

    /**
     * Attempts to decode one Ethernet frame from the beginning of @p available.
     *
     * The frame length is obtained from the IPv4 header. If a complete frame is available, constructs an
     * EthernetFrameView, invokes @p callback with the view, and advances the read head past the packet. If the Ethernet
     * or IPv4 data is incomplete, no callback is invoked and no bytes are consumed.
     *
     * @param available Unprocessed bytes beginning with an Ethernet frame.
     * @param callback Callback to invoke with the decoded EthernetFrameView.
     *
     * @return True if a complete Ethernet frame was decoded; false otherwise.
     *
     * @throws std::runtime_error The frame has an unsupported EtherType or IP version.
     * @throws std::runtime_error The frame has an invalid length.
     *
     * @note The EthernetFrameView supplied to @p callback refers to decoder-owned storage and remains valid only for
     * the duration of the callback invocation.
     */
    bool consume_ethernet_frame(std::span<const std::byte> available, TrafficDecoderCallback auto&& callback);

    /**
     * Discards the processed prefix of the internal input buffer.
     *
     * Moves any unprocessed bytes to the beginning of the buffer and resets the consumed-byte count.
     *
     * @post `consumed == 0`
     *
     * @throws std::bad_alloc Reorganising the buffer requires an allocation that fails.
     */
    void compact();

    std::vector<std::byte> buffer;
    std::size_t consumed = 0;
};

void TrafficDecoder::consume(const std::span<const std::byte> input, TrafficDecoderCallback auto&& callback)
{
    std::ranges::copy(input, std::back_inserter(buffer));

    while (consumed < buffer.size()) {
        // Get the subspan spanning from the beginning of the unconsumed bytes in the buffer to the end.
        const auto available = std::span<const std::byte>(buffer).subspan(consumed);

        if (available.size() < magic.size()) {
            // Four bytes are required to distinguish a CTFS record from an ethernet frame.
            compact();
            return;
        }

        // Send to suitable handler if CTFS record, or ethernet frame.
        if (std::ranges::equal(available.first(magic.size()), magic)) {
            if (!consume_ctfs_record(available, callback)) {
                compact();
                return;
            }
        } else {
            if (!consume_ethernet_frame(available, callback)) {
                compact();
                return;
            }
        }
    }

    compact();
}

void TrafficDecoder::finish(TrafficDecoderCallback auto&& callback)
{
    const auto remaining = std::span<const std::byte>{buffer}.subspan(consumed);

    const bool starts_with_magic =
        remaining.size() >= magic.size() && std::ranges::equal(remaining.first(magic.size()), magic);

    if (starts_with_magic)
        throw std::runtime_error("Input ended during a CTFS frame.");

    if (!remaining.empty())
        consume_ethernet_frame(buffer, callback);

    buffer.clear();
    consumed = 0;
}

bool TrafficDecoder::consume_ctfs_record(const std::span<const std::byte> available,
                                         TrafficDecoderCallback auto&& callback)
{
    static constexpr std::size_t header_size = 16;

    if (available.size() < header_size)
        return false;

    const auto sequence_number = decode_big_endian(available.subspan<4, 2>());
    const auto data_length = decode_big_endian(available.subspan<6, 2>());
    const auto record_size = header_size + static_cast<std::size_t>(data_length);

    if (available.size() < record_size)
        return false;

    const auto record_bytes = available.first(record_size);
    const CTFSPacketView packet_view{
        .bytes = record_bytes,
        .payload = record_bytes.subspan(header_size, data_length),
        .sequence_number = sequence_number,
        .data_length = data_length,
        .file_id = std::to_integer<std::uint8_t>(record_bytes[8]),
        .received_checksum = decode_big_endian(record_bytes.subspan<9, 4>())
    };

    std::invoke(callback, packet_view);
    consumed += record_size;
    return true;
}

bool TrafficDecoder::consume_ethernet_frame(const std::span<const std::byte> available,
                                            TrafficDecoderCallback auto&& callback)
{
    static constexpr std::size_t ethernet_header_size = 14;
    static constexpr std::size_t minimum_ipv4_header_size = 20;
    static constexpr std::uint16_t ipv4_ether_type = 0x800;

    constexpr auto minimum_frame_size = ethernet_header_size + minimum_ipv4_header_size;

    if (available.size() < minimum_frame_size)
        return false;

    const auto ether_type = decode_big_endian(available.subspan<12, 2>());
    if (ether_type != ipv4_ether_type)
        throw std::runtime_error("Unsupported ethernet protocol.");

    const auto ip_metadata = std::to_integer<std::uint8_t>(available[ethernet_header_size]);
    const auto ip_version = static_cast<std::uint8_t>(ip_metadata >> 4);
    const auto ip_header_size = static_cast<std::size_t>(ip_metadata & 0x0f) << 2;

    if (ip_version != 4)
        throw std::runtime_error("Unsupported IP version.");

    if (ip_header_size < minimum_ipv4_header_size)
        throw std::runtime_error("Invalid IPv4 header length.");

    const auto ip_total_length = decode_big_endian(available.subspan<16, 2>());
    if (ip_total_length < ip_header_size)
        throw std::runtime_error("Invalid IPv4 total length.");

    const auto frame_size = ethernet_header_size + ip_total_length;
    if (available.size() < frame_size)
        return false;

    const auto frame_bytes = available.first(frame_size);
    const auto ip_bytes = frame_bytes.subspan(ethernet_header_size, ip_total_length);

    const EthernetFrameView frame_view{
        .bytes = frame_bytes,

        .src_mac_prefix = frame_bytes.subspan<6, 3>(),
        .dst_mac_prefix = frame_bytes.first<3>(),

        .ip_header = ip_bytes.first(ip_header_size),
        .ip_payload = ip_bytes.subspan(ip_header_size),
        .ip_protocol = std::to_integer<std::uint8_t>(ip_bytes[9])
    };

    std::invoke(callback, frame_view);
    consumed += frame_size;
    return true;
}

} // namespace firestorm

#endif // FIRESTORM_TRAFFICDECODER_HPP
