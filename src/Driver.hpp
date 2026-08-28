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

#include <memory>
#include <vector>

#include "CTFSPacket.hpp"
#include "TrafficDecoder.hpp"

namespace firestorm
{

class IFrameRouter;

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
     * Creates a new Driver.
     *
     * Driver owns AsyncFrameRouter, which binds to sockets on fixed endpoints. Therefore, only one driver must exist at
     * one time. Note that sockets are bound lazily, so violations are not immediately apparent.
     *
     * @param classified_policy Should the AsyncFrameRouter use a classified routing policy?
     * @param reject_bad_checksums Should invalid checksums cause an exception?
     */
    explicit Driver(bool classified_policy, bool reject_bad_checksums);

    ~Driver() noexcept;

    Driver(const Driver&) = delete;
    Driver& operator=(const Driver&) = delete;
    Driver(Driver&&) = delete;
    Driver& operator=(Driver&&) = delete;

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
     * @param record A decoded non-owning view of the traffic record to process.
     *
     * @throws std::system_error Forwarding or storing the record fails.
     * @throws std::bad_alloc Processing requires an allocation that fails.
     *
     * @note Any storage referenced by @p record_view must remain valid for the entire duration of this call.
     */
    void despatch_record(TrafficRecord&& record);

    /**
     * Adds a CTFSPacket to its file packet group.
     *
     * If the packet describes a portion of a file that doesn't exist, the group is created to accommodate.
     *
     * @param ctfs_packet A constructed checksum-validated CTFSPacket.
     *
     * @throws std::bad_alloc Storing the CTFSPacket requires an allocation that fails.
     */
    void process_record(CTFSPacket&& ctfs_packet);

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

    bool reject_bad_checksums;                          /**< Should invalid checksums cause an exception? */
    std::unique_ptr<IFrameRouter> frame_router;         /**< Routing delegate for forwarding frame packets. */
    std::vector<std::vector<CTFSPacket>> packet_groups; /**< Unordered sets of packets, grouped by file ID. */
};

} // namespace firestorm

#endif // FIRESTORM_DRIVER_HPP
