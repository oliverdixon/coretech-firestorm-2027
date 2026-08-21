/**
 * @file
 *
 * CTFSRecordView implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "CTFSRecordView.hpp"

namespace firestorm
{

std::uint32_t CTFSPacketView::compute_checksum() const noexcept
{
    static constexpr std::uint32_t ctfs = ('C' << 24) | ('T' << 16) | ('F' << 8) | 'S';

    std::uint32_t accumulator = 0;
    const auto add_word = [&accumulator](const std::uint32_t word) {
        accumulator = std::rotl(accumulator + word, 1);
    };

    add_word(ctfs);                                                            // Word 1: "CTFS"
    add_word(static_cast<std::uint32_t>(sequence_number) << 16 | data_length); // Word 2: Sequence No. and Data Length
    add_word(static_cast<std::uint32_t>(file_id) << 24 | ctfs >> 8);           // Word 3: File ID and "CTF" (CTFS head)
    add_word(ctfs << 24);                                                      // Word 4: "S" (CTFS tail)

    std::size_t offset = 0;

    if (payload.size() > 4)
        for (; offset < payload.size() - 4; offset += 4) {
            const auto span = payload.subspan(offset, 4);
            add_word(get_word(span.first<4>()));
        }

    switch (payload.size() - offset) {
    case 1:
        add_word(std::to_integer<std::uint32_t>(payload[offset]) << 24);
        break;
    case 2:
        add_word(std::to_integer<std::uint32_t>(payload[offset]) << 24 |
                 std::to_integer<std::uint32_t>(payload[offset + 1]) << 16);
        break;
    case 3:
        add_word(std::to_integer<std::uint32_t>(payload[offset]) << 24 |
                 std::to_integer<std::uint32_t>(payload[offset + 1]) << 16 |
                 std::to_integer<std::uint32_t>(payload[offset + 2]) << 8);
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

constexpr std::uint32_t CTFSPacketView::get_word(std::span<const std::byte, 4> bytes) noexcept
{
    return (std::to_integer<std::uint32_t>(bytes[0]) << 24) | (std::to_integer<std::uint32_t>(bytes[1]) << 16) |
           (std::to_integer<std::uint32_t>(bytes[2]) << 8) | std::to_integer<std::uint32_t>(bytes[3]);
}

} // namespace firestorm
