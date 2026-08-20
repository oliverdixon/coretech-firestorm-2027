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

std::uint16_t PacketFilter::read_u16_be(const std::span<const std::byte, 2> bytes) noexcept
{
    return std::to_integer<std::uint16_t>(bytes[0]) << 8 | std::to_integer<std::uint16_t>(bytes[1]);
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
