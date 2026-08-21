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
#include <fstream>
#include <ranges>

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

    /**
     * Verifies that a sequence of packets contains a PDF byte stream.
     *
     * Confirms that every optional contains a packet, and performs a lightweight PDF validation by checking for the
     * PDF magic byte sequences. In the final 1024 bytes, the PDF EOF sequence is expected to occur at the end, except
     * any trailing whitespace.
     *
     * @param optional_packets Ordered packets comprising the PDF byte stream.
     *
     * @return A lazy non-owning view yielding each contained packet as an observing reference.
     *
     * @throws std::bad_optional_access Any element of @p optional_packets does not contain a packet.
     * @throws std::runtime_error The assembled byte stream does not begin with the PDF magic bytes.
     * @throws std::runtime_error The assembled byte stream does not end with the PDF EOF marker.
     *
     * @note The returned view does not own the packets. The @p optional_packets storage must remain alive and unchanged
     * while the view is used.
     *
     * @note This is a smoke test only; it does not validate the internal PDF structure.
     */
    static auto prepare_pdf_stream(std::span<const std::optional<CTFSPacket>> optional_packets)
    {
        using B = std::byte;

        static constexpr std::array pdf_magic_begin = {B{'%'}, B{'P'}, B{'D'}, B{'F'}, B{'-'}};
        static constexpr std::array pdf_magic_end = {B{'F'}, B{'O'}, B{'E'}, B{'%'}, B{'%'}};
        static constexpr std::size_t tail_limit = 1024;

        /*
         * Validate that all slots contain packets. optional::value will throw bad_optional_access if this is not the
         * case.
         */
        for (const auto& optional_packet : optional_packets)
            std::ignore = optional_packet.value();

        // Prepare the view of the unwrapped optionals.
        const auto packets =
            optional_packets | std::views::transform([](const std::optional<CTFSPacket>& opt) -> const CTFSPacket& {
                return opt.value();
            });

        const auto bytes = packets | std::views::transform(&CTFSPacket::observe_payload) | std::views::join;

        // Verify that we start with the PDF magic bytes.
        if (!std::ranges::equal(bytes | std::views::take(pdf_magic_begin.size()), pdf_magic_begin))
            throw std::runtime_error("Payload does not have a PDF header.");

        // Verify that the last non-empty bytes within the last 1024 section is the PDF EOF marker.
        constexpr auto is_padding = [](const std::byte b) {
            switch (b) {
            case B{'\0'}:
            case B{' '}:
            case B{'\t'}:
            case B{'\r'}:
            case B{'\n'}:
            case B{'\f'}:
                return true;
            default:
                return false;
            }
        };

        auto reversed_tail =
            bytes | std::views::reverse | std::views::take(tail_limit) | std::views::drop_while(is_padding);

        if (!std::ranges::equal(reversed_tail | std::views::take(pdf_magic_end.size()), pdf_magic_end))
            throw std::runtime_error("Payload does not have a PDF EOF marker.");

        return packets;
    }

    /**
     * Writes captured payloads in ascending sequence-number order for a single file.
     *
     * @param output Output file destination.
     * @param packets Ordered set of packets required to construct a PDF file at @p output.
     *
     * @throws std::ios_base::failure The entire PDF could not be reconstructed from @p packets.
     */
    template <std::ranges::input_range PacketStream>
    static void write_pdf(std::ofstream& output, const PacketStream& packets)
    {
        for (const auto& packet : packets) {
            const auto& payload = packet.observe_payload();
            output.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        }
    }

    bool drop_bad_checksums;  /**< Should invalid checksums be ignored? */
    FrameRouter frame_router; /**< Routing delegate for forwarding frame packets. */

    /** Ordered sets of packets, grouped by file ID. */
    std::vector<std::vector<std::optional<CTFSPacket>>> packet_groups;
};

} // namespace firestorm

#endif // FIRESTORM_DRIVER_HPP
