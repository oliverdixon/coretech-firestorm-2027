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

#include <limits>

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
     * provided in @p options.
     *
     * @param options Options describing the packet to construct.
     *
     * @return The complete serialised CTFS record in big-endian byte ordering.
     *
     * @throws std::invalid_argument The payload size exceeds the maximum value representable by a 16-bit data field.
     */
    [[nodiscard]] static std::vector<std::byte> make_ctfs_record(const Options& options)
    {
        if (options.payload.size() > std::numeric_limits<std::uint16_t>::max())
            throw std::invalid_argument("CTFS test payload is too large");

        const auto data_length = options.advertised_length.value_or(static_cast<std::uint16_t>(options.payload.size()));
        std::vector<std::byte> record;
        record.reserve(16 + options.payload.size());

        // Step 1.  Magic bytes.
        record.insert(record.end(), magic.begin(), magic.end());

        // Step 2.  Sequence number, data length, file ID, checksum placeholder, and padding.
        append_be16(record, options.sequence_number);
        append_be16(record, data_length);
        record.push_back(static_cast<std::byte>(options.file_id));
        record.insert(record.end(), magic.begin(), magic.end());
        record.insert(record.end(), options.padding.begin(), options.padding.end());

        // Step 3.  Payload.
        record.insert(record.end(), options.payload.begin(), options.payload.end());

        // Step 4.  Overwrite checksum now the entire packet is known.
        const auto reference_checksum = options.checksum.value_or(get_reference_checksum(record));
        record[9] = static_cast<std::byte>(reference_checksum >> 24);
        record[10] = static_cast<std::byte>(reference_checksum >> 16);
        record[11] = static_cast<std::byte>(reference_checksum >> 8);
        record[12] = static_cast<std::byte>(reference_checksum);

        return record;
    }

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
    [[nodiscard]] static std::uint32_t get_reference_checksum(const std::span<const std::byte> record) noexcept
    {
        const auto byte_at = [&record](const std::size_t offset) noexcept {
            return offset < record.size() ? record[offset] : std::byte{0};
        };

        std::uint32_t accumulator = 0;

        for (std::size_t offset = 0; offset < record.size(); offset += 4) {
            const auto word = std::to_integer<std::uint32_t>(byte_at(offset)) << 24 |
                              std::to_integer<std::uint32_t>(byte_at(offset + 1)) << 16 |
                              std::to_integer<std::uint32_t>(byte_at(offset + 2)) << 8 |
                              std::to_integer<std::uint32_t>(byte_at(offset + 3));

            accumulator = std::rotl(accumulator + word, 1);
        }

        return accumulator;
    }

    /**
     * Appends an unsigned 16-bit integer in big-endian byte order.
     *
     * @param output Destination byte buffer.
     * @param value Value to append.
     */
    static void append_be16(std::vector<std::byte>& output, const std::uint16_t value)
    {
        output.push_back(static_cast<std::byte>(value >> 8));
        output.push_back(static_cast<std::byte>(value));
    }

    /**
     * Appends an unsigned 32-bit integer in big-endian byte order.
     *
     * @param output Destination byte buffer.
     * @param value Value to append.
     */
    static void append_be32(std::vector<std::byte>& output, const std::uint32_t value)
    {
        output.push_back(static_cast<std::byte>(value >> 24));
        output.push_back(static_cast<std::byte>(value >> 16));
        output.push_back(static_cast<std::byte>(value >> 8));
        output.push_back(static_cast<std::byte>(value));
    }
};

} // namespace firestorm::test

#endif // FIRESTORM_CTFSPACKETSTREAMBUILDER_HPP
