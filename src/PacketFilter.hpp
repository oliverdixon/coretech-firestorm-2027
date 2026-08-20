/**
 * @file
 *
 * PacketFilter specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_PACKETFILTER_HPP
#define FIRESTORM_PACKETFILTER_HPP

#include <algorithm>
#include <stdexcept>

#include "CTFSPacket.hpp"

namespace firestorm
{

/**
 * Concept for callbacks that consume ordinary non-operational network data.
 *
 * A forwarding callback receives a temporary read-only view of bytes that have been determined not to belong to a CTFS
 * frame.
 *
 * @tparam F Callable type accepting `std::span<const std::byte>`.
 */
template <typename F>
concept ForwardCallback = std::invocable<F&, std::span<const std::byte>>;

/**
 * Concept for callbacks that consume decoded CTFS packets.
 *
 * @tparam C Callable type accepting a CTFSPacket rvalue.
 */
template <typename C>
concept CaptureCallback = std::invocable<C&, CTFSPacket>;

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
class PacketFilter
{
public:
    /**
     * Processes a chunk of input bytes.
     *
     * @param input Chunk of bytes from the input stream.
     * @param forward_callback Callback invoked synchronously for ordinary bytes.
     * @param capture_callback Callback invoked synchronously for each complete CTFS packet.
     */
    void consume(std::span<const std::byte> input,
                 ForwardCallback auto&& forward_callback,
                 CaptureCallback auto&& capture_callback);

    /**
     * Finishes processing after the input stream closes.
     *
     * Any retained bytes that cannot form a complete CTFS frame are forwarded as ordinary data through
     * @p forward_callback.
     *
     * @param forward_callback Callback invoked synchronously for remaining ordinary bytes.
     *
     * @throws std::runtime_error The stream ends during a CTFS frame.
     *
     * @post On success, the filter contains no buffered input and may be reused for another stream.
     */
    void finish(ForwardCallback auto&& forward_callback);

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
     * Number of bytes preceding the CTFS payload.
     */
    static constexpr std::size_t header_size = 16;

    /**
     * Decodes an unsigned 16-bit big-endian value.
     *
     * @param bytes Bytes containing the encoded value.
     *
     * @return Decoded value.
     */
    static std::uint16_t read_u16_be(std::span<const std::byte, 2> bytes) noexcept;

    /**
     * Removes processed bytes from the front of the buffer.
     *
     * @post `consumed == 0`
     */
    void compact();

    std::vector<std::byte> buffer;
    std::size_t consumed = 0;
};

void PacketFilter::consume(const std::span<const std::byte> input,
                           ForwardCallback auto&& forward_callback,
                           CaptureCallback auto&& capture_callback)
{
    std::ranges::copy(input, std::back_inserter(buffer));

    while (consumed < buffer.size()) {
        // Get the subspan spanning from the beginning of the unconsumed bytes in the buffer to the end.
        const auto available = std::span<const std::byte>(buffer).subspan(consumed);
        const auto match = std::ranges::search(available, magic);

        if (match.begin() == available.end()) {
            /*
             * No complete marker is available in the chunk.
             *
             * In a degenerate case, the marker may be crossing the payload boundary, such that the final three bytes
             * could be "C", "CT", or "CTF", followed by the remainder in the next chunk.
             *
             *
             */

            const auto retain_count = std::min(available.size(), magic.size() - 1);
            const auto forward_size = available.size() - retain_count;

            if (forward_size != 0) {
                forward_callback(available.first(forward_size));
                consumed += forward_size;
            }

            compact();
            return;
        }

        const auto prefix_size = std::ranges::distance(available.begin(), match.begin());

        if (prefix_size > 0) {
            // Forward the bytes prior to the beginning of the header.
            forward_callback(available.first(prefix_size));
            consumed += prefix_size;
        } else {
            if (available.size() < header_size) {
                compact();
                return;
            }

            // Otherwise, decode the header and payload and construct a packet. Send the packet to the capture callback.

            const auto sequence_number = read_u16_be(available.subspan<4, 2>());
            const auto payload_size = read_u16_be(available.subspan<6, 2>());
            const auto frame_size = header_size + payload_size;

            if (available.size() < frame_size) {
                compact();
                return;
            }

            const auto payload = available.subspan(header_size, payload_size);
            capture_callback(CTFSPacket(sequence_number, payload));
            consumed += frame_size;
        }
    }

    compact();
}

void PacketFilter::finish(ForwardCallback auto&& forward_callback)
{
    const auto remaining = std::span<const std::byte>{buffer}.subspan(consumed);

    const bool starts_with_magic =
        remaining.size() >= magic.size() && std::ranges::equal(remaining.first(magic.size()), magic);

    if (starts_with_magic)
        throw std::runtime_error("Input ended during a CTFS frame.");

    if (!remaining.empty())
        forward_callback(remaining);

    buffer.clear();
    consumed = 0;
}

} // namespace firestorm

#endif // FIRESTORM_PACKETFILTER_HPP
