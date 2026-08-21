/**
 * @file
 *
 * PDFWriter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-21
 */

#include "PDFWriter.hpp"

#include <fstream>

#include "Logger.hpp"

namespace firestorm
{

void PDFWriter::write_pdf(std::ofstream& output, const std::span<const CTFSPacket> packets)
{
    validate_pdf_stream(packets);

    std::size_t last_seq_num = -1;

    for (const auto& packet : packets)
        if (packet.get_sequence_number() == last_seq_num)
            LOG_F_WARNING("Ignoring CTFS packet with File ID {} and duplicated Sequence Number {}.",
                          packet.get_file_id(),
                          last_seq_num);
        else {
            last_seq_num = packet.get_sequence_number();

            const auto& payload = packet.observe_payload();
            output.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        }
}

void PDFWriter::validate_pdf_stream(const std::span<const CTFSPacket> packets)
{
    using B = std::byte;

    static constexpr std::array pdf_magic_begin = {B{'%'}, B{'P'}, B{'D'}, B{'F'}, B{'-'}};
    static constexpr std::array pdf_magic_end = {B{'F'}, B{'O'}, B{'E'}, B{'%'}, B{'%'}};
    static constexpr std::size_t tail_limit = 1024;

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
}

} // namespace firestorm
