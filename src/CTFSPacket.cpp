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

CTFSPacket::CTFSPacket(const std::uint16_t sequence_number, const std::span<const std::byte> payload) :
    sequence_number(sequence_number)
{
    this->payload.assign_range(payload);
}

CTFSPacket& CTFSPacket::operator=(CTFSPacket&& other) noexcept
{
    if (this == &other)
        return *this;

    sequence_number = other.sequence_number;
    payload = std::move(other.payload);

    return *this;
}

std::uint16_t CTFSPacket::get_sequence_number() const noexcept
{
    return sequence_number;
}

std::span<const std::byte> CTFSPacket::observe_payload() const noexcept
{
    return payload;
}

} // namespace firestorm
