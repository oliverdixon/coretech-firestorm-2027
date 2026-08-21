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
    /**
     * Creates a new Driver under the given FrameRouter::Policy.
     *
     * Driver owns FrameRouter, which binds to sockets on fixed endpoints. Therefore, only one driver must exist at one
     * time. Note that sockets are bound lazily, so violations are not immediately apparent.
     *
     * @param routing_policy The routing policy for FrameRouter.
     * @param drop_bad_checksums Should invalid checksums be ignored?
     */
    explicit Driver(FrameRouter::Policy routing_policy, bool drop_bad_checksums);

    /**
     * Processes the incoming stream and reconstructs the captured PDF.
     *
     * Reads until an orderly end-of-stream, forwards ordinary bytes, captures CTFS packets, and writes their payloads
     * in ascending sequence-number order to @p pdf_destination.
     *
     * @throws std::system_error A low-level socket operation failed.
     * @throws std::runtime_error The stream ends during a CTFS frame.
     * @throws std::runtime_error PDF output cannot be completed.
     * @throws std::ios_base::failure A destination file could not be opened.
     */
    void run();

private:
    /**
     * Processes a decoded traffic record.
     *
     * Despatches @p record_view to the overload appropriate for the contained record type.
     *
     * @param record_view A decoded non-owning view of the traffic record to process.
     *
     * @throws std::system_error Forwarding or storing the record fails.
     * @throws std::bad_alloc Processing requires an allocation that fails.
     *
     * @note Any storage referenced by @p record_view must remain valid for the entire duration of this call.
     */
    void process_record_view(const TrafficRecordView& record_view);

    /**
     * Processes a decoded CTFS packet.
     *
     * Validates the packet checksum and, if valid, adds the packet to the corresponding file reconstruction. Invalid or
     * duplicated packets are discarded.
     *
     * @param ctfs_packet_view A decoded non-owning view of the CTFS packet.
     *
     * @throws std::bad_alloc Storing the CTFSPacket requires an allocation that fails.
     *
     * @note Any storage referenced by @p ctfs_packet_view must remain valid for the entire duration of this call.
     */
    void process_record(const CTFSPacketView& ctfs_packet_view);

    /**
     * Processes a decoded Ethernet frame.
     *
     * Applies the configured frame-routing rules and forwards the frame to the appropriate endpoints.
     *
     * @param ethernet_frame_view A decoded non-owning view of the Ethernet frame.
     *
     * @throws std::system_error Frame forwarding to the endpoint fails.
     *
     * @note Any storage referenced by @p ethernet_frame_view must remain valid for the entire duration of this call.
     */
    void process_record(const EthernetFrameView& ethernet_frame_view);

    bool drop_bad_checksums;  /**< Should invalid checksums be ignored? */
    FrameRouter frame_router; /**< Routing delegate for forwarding frame packets. */

    /** Unordered sets of packets, grouped by file ID. */
    std::vector<std::vector<CTFSPacket>> packet_groups;
};

} // namespace firestorm

#endif // FIRESTORM_DRIVER_HPP
