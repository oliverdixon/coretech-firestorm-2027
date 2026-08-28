/**
 * @file
 *
 * CTFSPacketStreamBuilder specification
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#ifndef FIRESTORM_CTFSPACKETSTREAMBUILDER_HPP
#define FIRESTORM_CTFSPACKETSTREAMBUILDER_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace firestorm::test
{

/**
 * Builds binary records in the CTFS packet format.
 *
 * A CTFS record consists of a 16-byte header followed by a variable-length payload. Multi-byte integer fields are
 * encoded in big-endian ordering.
 *
 * @see CTFSPacket
 * @see CTFSRecordView
 */
class CTFSPacketStreamBuilder
{
public:
    /**
     * Options used when constructing a new record.
     *
     * This type does not own the payload bytes; the underlying storage must remain valid for the entire duration of the
     * call in which Options is used.
     */
    struct Options
    {
        std::uint16_t sequence_number = 0;              /**< Sequence number for the packet header. */
        std::uint8_t file_id = 0;                       /**< File ID for the packet header. */
        std::span<const std::byte> payload;             /**< Non-owning view into the payload data. */
        std::array<std::byte, 3> padding{};             /**< Zero padding bytes. */

        std::optional<std::uint16_t> advertised_length; /**< Advertised length; computed unless provided. */
        std::optional<std::uint32_t> checksum;          /**< Checksum; computed unless provided. */
    };

    /**
     * Produce the standard binary representation of a complete CTFS record.
     *
     * The function writes the CTFS header, appends the supplied payload, and encodes the reference checksum unless
     * provided in @p options. "Magic numbers" are intentionally used liberally in this reference implementation, since
     * it must be trivially verifiable against the CTFS packet specification.
     *
     * @param options Options describing the packet to construct.
     *
     * @return The complete serialised CTFS record in big-endian byte ordering.
     *
     * @throws std::invalid_argument The payload size exceeds the maximum value representable by a 16-bit data field.
     */
    [[nodiscard]] static std::vector<std::byte> make_ctfs_record(const Options& options);

private:
    /**
     * Magic bytes for the header prefix and checksum placeholder.
     */
    static constexpr std::array magic{
        std::byte{'C'},
        std::byte{'T'},
        std::byte{'F'},
        std::byte{'S'},
    };

    /**
     * Calculates the reference checksum for a serialised CTFS record.
     *
     * The record is processed a sa sequence of big-endian 32-bit words. During calculation, checksum bytes 9 through 12
     * are treated as the byte sequence `CTFS`. An incomplete final word is padded with zero bytes.
     *
     * For each word, the word is added to a 32-bit accumulator and the result is rotated left by one bit.
     *
     * @param record Complete serialised record for which to calculate the checksum.
     *
     * @return The calculated 32-bit checksum.
     */
    [[nodiscard]] static std::uint32_t get_reference_checksum(std::span<const std::byte> record) noexcept;

    /**
     * Appends an unsigned 16-bit integer in big-endian byte order.
     *
     * @param output Destination byte buffer.
     * @param value Value to append.
     */
    static void append_be16(std::vector<std::byte>& output, std::uint16_t value);

    /**
     * Appends an unsigned 32-bit integer in big-endian byte order.
     *
     * @param output Destination byte buffer.
     * @param value Value to append.
     */
    static void append_be32(std::vector<std::byte>& output, std::uint32_t value);
};

} // namespace firestorm::test

#endif // FIRESTORM_CTFSPACKETSTREAMBUILDER_HPP
