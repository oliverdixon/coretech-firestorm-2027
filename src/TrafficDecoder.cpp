/**
 * @file
 *
 * PacketFilter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "TrafficDecoder.hpp"

namespace firestorm
{

TrafficDecoder::TrafficDecoder(const bool reject_bad_checksums)
    : reject_bad_checksums(reject_bad_checksums)
{
}

void TrafficDecoder::finish()
{
    const auto remaining = std::span<const std::byte>{buffer}.subspan(consumed);

    if (!remaining.empty())
        throw std::runtime_error("Input ended during an incomplete traffic record.");

    buffer.clear();
    consumed = 0;
}

std::uint16_t TrafficDecoder::decode_big_endian(const std::span<const std::byte, 2> bytes) noexcept
{
    return std::to_integer<std::uint16_t>(bytes[0]) << 8 | std::to_integer<std::uint16_t>(bytes[1]);
}

std::uint32_t TrafficDecoder::decode_big_endian(const std::span<const std::byte, 4> bytes) noexcept
{
    return std::to_integer<std::uint32_t>(bytes[0]) << 24 | std::to_integer<std::uint32_t>(bytes[1]) << 16 |
           std::to_integer<std::uint32_t>(bytes[2]) << 8 | std::to_integer<std::uint32_t>(bytes[3]);
}

void TrafficDecoder::compact()
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
