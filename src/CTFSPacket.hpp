/**
 * @file
 *
 * CTFSPacket specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_CTFSPACKET_HPP
#define FIRESTORM_CTFSPACKET_HPP

#include <compare>
#include <cstdint>
#include <span>
#include <vector>

namespace firestorm
{

/**
 * @todo Document
 */
struct CTFSPacket
{
    CTFSPacket(const std::uint16_t sequence_number, const std::span<const std::byte> payload) :
        sequence_number(sequence_number)
    {
        this->payload.assign_range(payload);
    }

    friend std::strong_ordering operator<=>(const CTFSPacket& lhs, const CTFSPacket& rhs) noexcept;
    friend bool operator==(const CTFSPacket& lhs, const CTFSPacket& rhs) noexcept;

    CTFSPacket(CTFSPacket&) = delete;
    CTFSPacket& operator=(CTFSPacket&) = delete;

    CTFSPacket(CTFSPacket&&) = default;

    std::uint16_t sequence_number;
    std::vector<std::byte> payload;
};

} // namespace firestorm

#endif // FIRESTORM_CTFSPACKET_HPP
