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
 * @todo Document
 */
struct CTFSPacketView
{
    std::span<const std::byte> bytes;
    std::span<const std::byte> payload;

    std::uint16_t sequence_number;
    std::uint16_t data_length;
    std::uint8_t file_id;
    std::uint32_t received_checksum;

    [[nodiscard]] std::uint32_t compute_checksum() const noexcept;

    [[nodiscard]] CTFSPacket make_packet() const;

private:
    static constexpr std::uint32_t get_word(std::span<const std::byte, 4> bytes) noexcept;
};

} // namespace firestorm

#endif // FIRESTORM_CTFSRECORDVIEW_HPP
