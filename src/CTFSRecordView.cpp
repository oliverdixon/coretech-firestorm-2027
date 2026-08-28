/**
 * @file
 *
 * CTFSRecordView implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "CTFSRecordView.hpp"

#include <bit>

namespace firestorm
{

std::uint32_t CTFSPacketView::compute_checksum() const noexcept
{
    static constexpr std::uint32_t ctfs =
        (std::uint32_t{'C'} << 24U) | (std::uint32_t{'T'} << 16U) | (std::uint32_t{'F'} << 8U) | std::uint32_t{'S'};

    std::uint32_t accumulator = 0;
    const auto add_word = [&accumulator](const std::uint32_t word) {
        accumulator = std::rotl(accumulator + word, 1);
    };

    add_word(ctfs);                                                             // Word 1: "CTFS"
    add_word(static_cast<std::uint32_t>(sequence_number) << 16U | received_data_length); // Word 2: Sequence No. and Data Length
    add_word(static_cast<std::uint32_t>(file_id) << 24U | ctfs >> 8U);          // Word 3: File ID and "CTF" (CTFS head)
    add_word(ctfs << 24U | std::to_integer<std::uint32_t>(padding[0]) << 16U |
             std::to_integer<std::uint32_t>(padding[1]) << 8U |
             std::to_integer<std::uint32_t>(padding[2])); // Word 4: "S" (CTFS tail) and padding

    std::size_t offset = 0;

    if (payload.size() > 4)
        for (; offset < payload.size() - 4; offset += 4) {
            const auto span = payload.subspan(offset, 4);
            add_word(get_word(span.first<4>()));
        }

    switch (payload.size() - offset) {
        case 1:
            add_word(std::to_integer<std::uint32_t>(payload[offset]) << 24U);
            break;
        case 2:
            add_word(std::to_integer<std::uint32_t>(payload[offset]) << 24U |
                     std::to_integer<std::uint32_t>(payload[offset + 1]) << 16U);
            break;
        case 3:
            add_word(std::to_integer<std::uint32_t>(payload[offset]) << 24U |
                     std::to_integer<std::uint32_t>(payload[offset + 1]) << 16U |
                     std::to_integer<std::uint32_t>(payload[offset + 2]) << 8U);
            break;
        case 4:
            add_word(get_word(payload.last<4>()));
            break;
        default:;
    }

    return accumulator;
}

CTFSPacket CTFSPacketView::make_packet() const
{
    return {sequence_number, file_id, payload};
}

constexpr std::uint32_t CTFSPacketView::get_word(const std::span<const std::byte, 4> bytes) noexcept
{
    return (std::to_integer<std::uint32_t>(bytes[0]) << 24U) | (std::to_integer<std::uint32_t>(bytes[1]) << 16U) |
           (std::to_integer<std::uint32_t>(bytes[2]) << 8U) | std::to_integer<std::uint32_t>(bytes[3]);
}

} // namespace firestorm
