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

namespace firestorm
{

/**
 * @todo Document
 */
class CTFSPacketStreamBuilder
{
public:
    struct Options
    {
        std::uint16_t sequence_number = 0;
        std::uint8_t file_id = 0;
        std::span<const std::byte> payload;
        std::array<std::byte, 3> padding{};

        std::optional<std::uint16_t> advertised_length;
        std::optional<std::uint32_t> checksum;
    };

    [[nodiscard]] static std::vector<std::byte> make_ctfs_record(const Options& options)
    {
        if (options.payload.size() > std::numeric_limits<std::uint16_t>::max())
            throw std::invalid_argument("CTFS test payload is too large");

        const auto data_length = options.advertised_length.value_or(static_cast<std::uint16_t>(options.payload.size()));
        std::vector<std::byte> record;
        record.reserve(16 + options.payload.size());

        // Step 1.  Magic bytes.
        record.insert(record.end(), {std::byte{'C'}, std::byte{'T'}, std::byte{'F'}, std::byte{'S'}});

        // Step 2.  Sequence number, data length, file ID, checksum placeholder, and padding.
        append_be16(record, options.sequence_number);
        append_be16(record, data_length);
        record.push_back(static_cast<std::byte>(options.file_id));
        append_be32(record, 0);
        record.insert(record.end(), options.padding.begin(), options.padding.end());

        // Step 3.  Payload.
        record.insert(record.end(), options.payload.begin(), options.payload.end());

        // Step 4.  Overwrite checksum now the entire packet is known.
        const auto reference_checksum = get_reference_checksum(record);
        record[9] = static_cast<std::byte>(reference_checksum >> 24);
        record[10] = static_cast<std::byte>(reference_checksum >> 16);
        record[11] = static_cast<std::byte>(reference_checksum >> 8);
        record[12] = static_cast<std::byte>(reference_checksum);

        return record;
    }

private:
    [[nodiscard]] static std::uint32_t get_reference_checksum(const std::span<const std::byte> record)
    {
        const auto byte_at = [&record](const std::size_t offset) {
            static constexpr std::array replacement{
                std::byte{'C'},
                std::byte{'T'},
                std::byte{'F'},
                std::byte{'S'},
            };

            if (offset >= 9 && offset <= 12)
                return replacement[offset - 9];

            if (offset >= record.size())
                return std::byte{0};

            return record[offset];
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

    static void append_be16(std::vector<std::byte>& output, const std::uint16_t value)
    {
        output.push_back(static_cast<std::byte>(value >> 8));
        output.push_back(static_cast<std::byte>(value));
    }

    static void append_be32(std::vector<std::byte>& output, const std::uint32_t value)
    {
        output.push_back(static_cast<std::byte>(value >> 24));
        output.push_back(static_cast<std::byte>(value >> 16));
        output.push_back(static_cast<std::byte>(value >> 8));
        output.push_back(static_cast<std::byte>(value));
    }
};

} // namespace firestorm

#endif // FIRESTORM_CTFSPACKETSTREAMBUILDER_HPP
