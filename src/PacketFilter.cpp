/**
 * @file
 *
 * PacketFilter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "PacketFilter.hpp"

namespace firestorm
{

std::uint16_t PacketFilter::read_u16_be(const std::span<const std::byte> bytes, const std::size_t offset) noexcept
{
    return std::to_integer<std::uint16_t>(bytes[offset]) << 8 | std::to_integer<std::uint16_t>(bytes[offset + 1]);
}

void PacketFilter::compact()
{
    if (consumed == 0)
        return;

    buffer.erase(buffer.begin(), std::ranges::next(buffer.begin(), static_cast<std::ptrdiff_t>(consumed)));
    consumed = 0;
}

} // namespace firestorm
