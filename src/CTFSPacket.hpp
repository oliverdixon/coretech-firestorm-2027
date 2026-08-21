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

#include <cstdint>
#include <span>
#include <vector>

namespace firestorm
{

/**
 * A decoded CTFS packet.
 *
 * A CTFSPacket contains the packet's sequence number, the ID of the file to which it belongs, and an owned copy of its
 * payload. The CTFS magic bytes and the remainder of the transmission header are not included in the stored payload.
 *
 * Packets are ordered and compared for equality solely by sequence number and file ID.
 */
class CTFSPacket
{
public:
    /**
     * Constructs a packet from its decoded fields.
     *
     * Copies the contents of @p payload into storage owned by the packet. The supplied span therefore does not need to
     * remain valid after the constructor returns.
     *
     * @param sequence_number Packet sequence number, in host byte order.
     * @param file_id ID of the PDF to which the packet should be used to reconstruct.
     * @param payload Packet payload, excluding the CTFS header.
     *
     * @throws std::bad_alloc Storage for the payload cannot be allocated.
     */
    CTFSPacket(std::uint16_t sequence_number, std::uint8_t file_id, std::span<const std::byte> payload);

    /**
     * Compares two packets by sequence number.
     *
     * @param lhs The left-hand packet.
     * @param rhs The right-hand packet.
     *
     * @return Strong ordering of @p lhs relative to @p rhs.
     */
    friend std::strong_ordering operator<=>(const CTFSPacket& lhs, const CTFSPacket& rhs) noexcept;

    /**
     * Determines equality between two packets by their sequence numbers.
     *
     * @param lhs The left-hand packet.
     * @param rhs The right-hand packet.
     *
     * @return Do both packets have the same sequence number?
     */
    friend bool operator==(const CTFSPacket& lhs, const CTFSPacket& rhs) noexcept;

    CTFSPacket(const CTFSPacket&) = delete;
    CTFSPacket& operator=(const CTFSPacket&) = delete;

    CTFSPacket(CTFSPacket&&) noexcept = default;
    CTFSPacket& operator=(CTFSPacket&& other) noexcept;

    /**
     * Retrieves the packet sequence number.
     * @return Sequence number.
     */
    [[nodiscard]] std::uint16_t get_sequence_number() const noexcept;

    /**
     * Retrieves the file ID to which the packet belongs.
     * @return File ID number.
     */
    [[nodiscard]] std::uint8_t get_file_id() const noexcept;

    /**
     * Retrieves an observing span of the packet payload.
     * @return A non-owning view of the packet payload.
     */
    [[nodiscard]] const std::vector<std::byte>& observe_payload() const noexcept;

private:
    std::uint16_t sequence_number;
    std::uint8_t file_id;
    std::vector<std::byte> payload;
};

} // namespace firestorm

#endif // FIRESTORM_CTFSPACKET_HPP
