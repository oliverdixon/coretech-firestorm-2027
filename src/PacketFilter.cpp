/**
 * @file
 *
 * PacketFilter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "PacketFilter.hpp"

#include <limits>

namespace firestorm
{

PacketFilter::PacketFilter()
{
    buffer.reserve(header_size + std::numeric_limits<std::uint16_t>::max());
}

std::uint16_t PacketFilter::decode_big_endian(const std::span<const std::byte, 2> bytes) noexcept
{
    return std::to_integer<std::uint16_t>(bytes[0]) << 8 | std::to_integer<std::uint16_t>(bytes[1]);
}

std::uint32_t PacketFilter::decode_big_endian(const std::span<const std::byte, 4> bytes) noexcept
{
    return std::to_integer<std::uint32_t>(bytes[0]) << 24 |
           std::to_integer<std::uint32_t>(bytes[1]) << 16 |
           std::to_integer<std::uint32_t>(bytes[2]) << 8 |
           std::to_integer<std::uint32_t>(bytes[3]);
}

std::optional<CTFSPacket> PacketFilter::parse_packet(const std::span<const std::byte> data)
{
    const auto sequence_number = decode_big_endian(data.subspan<4, 2>());
    const auto payload_size = decode_big_endian(data.subspan<6, 2>());
    const auto file_id = static_cast<std::uint8_t>(data[8]);
    const auto checksum = decode_big_endian(data.subspan<9, 4>());
    const auto frame_size = header_size + payload_size;

    if (data.size() < frame_size) {
        compact();
        return std::nullopt;
    }

    consumed += frame_size;
    return CTFSPacket(sequence_number, file_id, data.subspan(header_size, payload_size));
}

void PacketFilter::compact()
{
    if (consumed == buffer.size()) {
        buffer.clear();
        consumed = 0;
    } else if (consumed >= 64 * 1024 && consumed >= buffer.size() / 2) {
        std::ranges::move(std::span{buffer}.subspan(consumed), buffer.begin());
        buffer.resize(buffer.size() - consumed);
        consumed = 0;
    }
}

} // namespace firestorm
