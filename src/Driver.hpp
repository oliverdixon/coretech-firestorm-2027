/**
 * @file
 *
 * Driver specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_DRIVER_HPP
#define FIRESTORM_DRIVER_HPP

#include <filesystem>
#include <set>

#include "CTFSPacket.hpp"
#include "TCPConnection.hpp"

namespace firestorm
{

/**
 * @todo Document
 */
class Driver
{
public:
    void run(const std::filesystem::path& pdf_destination);

private:
    void forward_bytes(std::span<const std::byte> bytes);

    void capture_packet(CTFSPacket&& packet);

    void write_pdf(const std::filesystem::path& pdf_destination) const;

    TCPConnection incoming{5733};
    // TCPConnection outgoing{"127.0.0.1", 5744}; // TODO

    std::size_t forwarded_bytes_count = 0;
    std::set<CTFSPacket> packets;
};

} // namespace firestorm

#endif // FIRESTORM_DRIVER_HPP
