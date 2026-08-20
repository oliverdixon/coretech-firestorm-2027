/**
 * @file
 *
 * CTFSPacket implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "CTFSPacket.hpp"

namespace firestorm
{

std::strong_ordering operator<=>(const CTFSPacket& lhs, const CTFSPacket& rhs) noexcept
{
    return lhs.sequence_number <=> rhs.sequence_number;
}

bool operator==(const CTFSPacket& lhs, const CTFSPacket& rhs) noexcept
{
    return lhs.sequence_number == rhs.sequence_number;
}

} // namespace firestorm
