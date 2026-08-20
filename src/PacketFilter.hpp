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

#include <array>
#include <set>
#include <span>
#include <vector>

#include "CTFSPacket.hpp"

namespace firestorm
{

/**
 * @todo Document
 */
class PacketFilter
{
public:
    template <typename Forward, typename Capture>
        requires std::invocable<Forward&, std::span<const std::byte>> && std::invocable<Capture&, CTFSPacket>
    void consume(std::span<const std::byte> input, Forward&& forward_callback, Capture&& capture_callback);

private:
    static constexpr std::array magic{
        std::byte{0x43}, // 'C',
        std::byte{0x54}, // 'T',
        std::byte{0x46}, // 'F'
        std::byte{0x53}, // 'S'
    };

    static constexpr std::size_t header_size = 16;

    static std::uint16_t read_u16_be(std::span<const std::byte> bytes, std::size_t offset) noexcept;

    void compact();

    std::vector<std::byte> buffer;
    std::size_t consumed = 0;
};

template <typename Forward, typename Capture>
    requires std::invocable<Forward&, std::span<const std::byte>> && std::invocable<Capture&, CTFSPacket>
void PacketFilter::consume(std::span<const std::byte> input, Forward&& forward_callback, Capture&& capture_callback)
{
    std::ranges::copy(input, std::back_inserter(buffer));

    // TODO logic needs tidying.

    for (;;) {
        const auto available = std::span<const std::byte>(buffer).subspan(consumed);
        const auto match = std::ranges::search(available, magic);

        if (match.begin() == available.end()) {
            constexpr std::size_t retained = magic.size() - 1;
            const std::size_t forward_size = available.size() > retained ? available.size() - retained : 0;

            if (forward_size != 0) {
                forward_callback(available.first(forward_size));
                consumed += forward_size;
            }

            compact();
            return;
        }

        const std::size_t prefix_size = std::ranges::distance(available.begin(), match.begin());
        if (prefix_size != 0) {
            forward_callback(available.first(prefix_size));
            consumed += prefix_size;
            continue;
        }

        if (available.size() < header_size) {
            compact();
            return;
        }

        const auto sequence_number = read_u16_be(available, 4);
        const auto payload_size = read_u16_be(available, 6);
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

} // namespace firestorm

#endif // FIRESTORM_PACKETFILTER_HPP
