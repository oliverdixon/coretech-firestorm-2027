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
#include <map>
#include <optional>
#include <set>

#include "CTFSPacket.hpp"
#include "TCPConnection.hpp"

namespace firestorm
{

/**
 * Coordinates CTFS packet extraction, non-operational data forwarding, and PDF reconstruction.
 *
 * Driver reads a blocking incoming byte stream over TCP, forwards bytes outside CTFS frames to the configured local
 * endpoint, and stores captured CTFS packets by sequence number. After the peer closes the connection, the captured
 * payloads are written to a PDF in sequence-number order.
 */
class Driver
{
public:
    /**
     * Processes the incoming stream and reconstructs the captured PDF.
     *
     * Reads until an orderly end-of-stream, forwards ordinary bytes, captures CTFS packets, and writes their payloads
     * in ascending sequence-number order to @p pdf_destination.
     *
     * @throws std::system_error A low-level socket operation failed.
     * @throws std::runtime_error The stream ends during a CTFS frame.
     * @throws std::runtime_error Duplicate sequence numbers are encountered.
     * @throws std::runtime_error PDF output cannot be completed.
     * @throws std::ios_base::failure A destination file could not be opened.
     */
    void run();

private:
    static constexpr std::uint16_t incoming_port = 5733; /**< TCP port on which to receive data. */
    static constexpr std::uint16_t outgoing_port = 5744; /**< TCP port on which to send non-operational data. */
    static constexpr auto endpoint_addr = "127.0.0.1";   /**< Network address of the endpoint to forward data. */

    /**
     * Forwards ordinary stream bytes to the endpoint.
     *
     * The outgoing connection is established lazily on the first non-empty block.
     *
     * @param bytes Bytes to forward.
     *
     * @throws std::system_error The outgoing connection cannot be established or could not receive the entire @p bytes.
     */
    void forward_bytes(std::span<const std::byte> bytes);

    /**
     * Stores a captured CTFS packet for ordered reconstruction.
     *
     * @param packet Packet to capture.
     *
     * @throws std::runtime_error A packet with the same sequence number has already been captured.
     * @throws std::bad_alloc Storage cannot be allocated.
     */
    void capture_packet(CTFSPacket&& packet);

    /**
     * Writes captured payloads in ascending sequence-number order for a single file.
     *
     * @param output Output file destination.
     * @param packets Ordered set of packets required to construct a PDF file at @p output.
     *
     * @throws std::ios_base::failure The entire PDF could not be reconstructed from @p packets.
     */
    static void write_pdf(std::ofstream& output, const std::set<CTFSPacket>& packets);

    std::optional<TCPConnection> outgoing; /**< Lazily established connection to peer endpoint to receive data. */
    std::map<std::uint8_t, std::set<CTFSPacket>> packet_groups; /**< Ordered sets of packets, grouped by file ID. */
};

} // namespace firestorm

#endif // FIRESTORM_DRIVER_HPP
