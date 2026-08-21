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
#include <set>

#include "CTFSPacket.hpp"
#include "FrameRouter.hpp"
#include "TrafficDecoder.hpp"

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
    explicit Driver(FrameRouter::Policy routing_policy);

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
    static constexpr auto endpoint_addr = "127.0.0.1";

    static constexpr std::uint16_t tcp_port = 5744;
    static constexpr std::uint16_t monitor_port = 5745;
    static constexpr std::uint16_t udp_port = 5746;

    void process_record_view(const TrafficRecordView& record_view);

    void process_record(const CTFSPacketView& ctfs_packet_view);
    void process_record(const EthernetFrameView& ethernet_frame_view);

    /**
     * Writes captured payloads in ascending sequence-number order for a single file.
     *
     * @param output Output file destination.
     * @param packets Ordered set of packets required to construct a PDF file at @p output.
     *
     * @throws std::ios_base::failure The entire PDF could not be reconstructed from @p packets.
     */
    static void write_pdf(std::ofstream& output, const std::set<CTFSPacket>& packets);

    FrameRouter frame_router;
    std::map<std::uint8_t, std::set<CTFSPacket>> packet_groups; /**< Ordered sets of packets, grouped by file ID. */
};

} // namespace firestorm

#endif // FIRESTORM_DRIVER_HPP
