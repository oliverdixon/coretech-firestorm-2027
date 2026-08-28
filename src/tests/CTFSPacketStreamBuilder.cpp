/**
 * @file
 *
 * CTFSPacketStreamBuilder implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-28
 */

#include "CTFSPacketStreamBuilder.hpp"

#include <limits>
#include <stdexcept>

std::vector<std::byte> firestorm::test::CTFSPacketStreamBuilder::make_ctfs_record(const Options& options)
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
    record[9] = static_cast<std::byte>(reference_checksum >> 24U);
    record[10] = static_cast<std::byte>(reference_checksum >> 16U);
    record[11] = static_cast<std::byte>(reference_checksum >> 8U);
    record[12] = static_cast<std::byte>(reference_checksum);

    return record;
}

std::uint32_t firestorm::test::CTFSPacketStreamBuilder::get_reference_checksum(
    std::span<const std::byte> record) noexcept
{
    const auto byte_at = [&record](const std::size_t offset) noexcept {
        return offset < record.size() ? record[offset] : std::byte{0};
    };

    std::uint32_t accumulator = 0;

    for (std::size_t offset = 0; offset < record.size(); offset += 4) {
        const auto word = std::to_integer<std::uint32_t>(byte_at(offset)) << 24U |
                          std::to_integer<std::uint32_t>(byte_at(offset + 1)) << 16U |
                          std::to_integer<std::uint32_t>(byte_at(offset + 2)) << 8U |
                          std::to_integer<std::uint32_t>(byte_at(offset + 3));

        accumulator = std::rotl(accumulator + word, 1);
    }

    return accumulator;
}

void firestorm::test::CTFSPacketStreamBuilder::append_be16(std::vector<std::byte>& output, const std::uint16_t value)
{
    output.push_back(static_cast<std::byte>(value >> 8U));
    output.push_back(static_cast<std::byte>(value));
}

void firestorm::test::CTFSPacketStreamBuilder::append_be32(std::vector<std::byte>& output, const std::uint32_t value)
{
    output.push_back(static_cast<std::byte>(value >> 24U));
    output.push_back(static_cast<std::byte>(value >> 16U));
    output.push_back(static_cast<std::byte>(value >> 8U));
    output.push_back(static_cast<std::byte>(value));
}
