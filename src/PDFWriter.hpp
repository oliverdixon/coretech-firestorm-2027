/**
 * @file
 *
 * PDFWriter specification
 *
 * @author Oliver Dixon
 * @date 2026-08-21
 */

#ifndef FIRESTORM_PDFWRITER_HPP
#define FIRESTORM_PDFWRITER_HPP

#include <fstream>
#include <optional>
#include <ranges>
#include <span>

#include "CTFSPacket.hpp"

namespace firestorm
{

/**
 * Validates and produces PDF files from CTFSPacket streams.
 */
class PDFWriter
{
public:
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
};

} // namespace firestorm

#endif // FIRESTORM_PDFWRITER_HPP
