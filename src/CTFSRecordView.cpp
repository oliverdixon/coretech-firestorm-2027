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
    // TODO
    return 0;
}

bool CTFSPacketView::has_valid_checksum() const noexcept
{
    return received_checksum == compute_checksum();
}

CTFSPacket CTFSPacketView::make_packet() const
{
    return {sequence_number, file_id, payload};
}

} // namespace firestorm
