/**
 * @file
 *
 * CTFSRecordView specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_CTFSRECORDVIEW_HPP
#define FIRESTORM_CTFSRECORDVIEW_HPP

#include <cstddef>
#include <cstdint>
#include <span>

#include "CTFSPacket.hpp"

namespace firestorm
{

/**
 * Provides a non-owning decoded view of a CTFS packet.
 *
 * The view exposes both the complete encoded packet and its payload, together with the fields decoded from the packet
 * header. The referenced byte storage must remain valid and unchanged for the lifetime of this object.
 *
 * @note This type does not own the storage referenced by @ref bytes or @ref payload.
 */
struct CTFSPacketView
{
    /** Complete encoded CTFS packet including its header and payload. */
    std::span<const std::byte> bytes;

    /** Portion of the header for the padding; should always be zero. */
    std::span<const std::byte, 3> padding;

    /** Packet payload excluding the CTFS header. */
    std::span<const std::byte> payload;

    std::uint16_t sequence_number;
    std::uint16_t data_length;
    std::uint8_t file_id;
    std::uint32_t received_checksum;

    /**
     * Computes the checksum of the packet.
     *
     * The checksum is calculated over the decoded header fields and payload, substituting the protocol-defined magic
     * value by the checksum field. A final partial 32-bit word is padded with zero bytes.
     *
     * @return The computed checksum in host byte order.
     */
    [[nodiscard]] std::uint32_t compute_checksum() const noexcept;

    /**
     * Creates an owning packet from this view.
     *
     * Copies the decoded packet metadata and payload into a CTFSPacket whose contents remain valid independently of the
     * storage referenced by this view.
     *
     * @return An owning representation of the viewed packet.
     * @throws std::bad_alloc Storage for the packet payload cannot be allocated.
     */
    [[nodiscard]] CTFSPacket make_packet() const;

private:
    /**
     * Decodes a four-byte big-endian word.
     *
     * @param bytes The four bytes to decode, most significant byte first.
     * @return The decoded 32-bit value in host byte order.
     */
    static constexpr std::uint32_t get_word(std::span<const std::byte, 4> bytes) noexcept;
};

} // namespace firestorm

#endif // FIRESTORM_CTFSRECORDVIEW_HPP
