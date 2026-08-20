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

using TrafficRecordView = std::variant<CTFSPacketView, EthernetFrameView>;

/**
 * Concept for callbacks that consume TrafficRecordView results.
 *
 * @tparam F Callable type accepting TrafficRecordView.
 */
template <typename F>
concept TrafficDecoderCallback = std::invocable<F&, const TrafficRecordView&>;

/**
 * Separates CTFS frames from an arbitrary stream of bytes.
 *
 * PacketFilter incrementally processes byte chunks that need not correspond to TCP packets, writes, or CTFS frame
 * boundaries. Ordinary bytes are reported through a ForwardCallback, while complete CTFS frames are decoded and
 * reported through a CaptureCallback.
 *
 * The filter retains incomplete markers, headers, and payloads between calls @ref consume. After the input stream
 * stream closes, @ref finish must be called to flush any remaining ordinary bytes and detect a truncated CTFS frame.
 *
 * A CTFS frame is interpreted as:
 *
 * Offset | Size | Description
 * ------ | ---- | -----------
 * 0      | 4    | "CTFS" magic bytes
 * 4      | 2    | Sequence marker, in network byte order
 * 6      | 2    | Payload length, in network byte order
 * 8      | 8    | Reserved
 * 16     | ...  | Payload
 *
 * @note The PacketFilter is independent of the source and destination transports.
 *
 * @note Spans passed to the ForwardCallback refer to internal storage and remain valid only for the duration of the
 * callback invocation.
 *
 * @warning The PacketFilter is neither thread-safe nor reentrant. A callback must not call @ref consume or @ref finish
 * on the same filter.
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

    bool consume_ctfs_record(std::span<const std::byte> available, TrafficDecoderCallback auto&& callback);
    bool consume_ethernet_frame(std::span<const std::byte> available, TrafficDecoderCallback auto&& callback);

    /**
     * Removes processed bytes from the front of the buffer.
     *
     * @post `consumed == 0`
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

        .src_mac = frame_bytes.subspan<6, 6>(),
        .dst_mac = frame_bytes.first<6>(),

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
