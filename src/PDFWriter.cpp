/**
 * @file
 *
 * PDFWriter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-21
 */

#include "PDFWriter.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ranges>
#include <stdexcept>
#include <vector>

#include "CTFSPacket.hpp"
#include "Logger.hpp"

namespace firestorm
{

void PDFWriter::write_pdf(std::ostream& output, const std::span<const CTFSPacket> packets)
{
    if (packets.empty())
        throw std::runtime_error("Packet collection was empty.");

    const auto packet_stream = build_pdf_packet_stream(packets);
    validate_pdf_payload_stream(packet_stream);

    for (const auto packet : packet_stream) {
        const auto& payload = packet.get().observe_payload();
        output.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));

        if (output.fail())
            // If output didn't have exceptions enabled by the caller, throw one here.
            throw std::ios_base::failure("Could not write PDF payload.");
    }
}
std::vector<std::reference_wrapper<const CTFSPacket>> PDFWriter::build_pdf_packet_stream(
    std::span<const CTFSPacket> packets)
{
    const auto working_file_id = packets.begin()->get_file_id();
    std::vector<std::reference_wrapper<const CTFSPacket>> stream;

    stream.reserve(packets.size());
    stream.emplace_back(*packets.begin());

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-reference" // False positive on g++-13.

    for (const auto& packet : packets | std::views::drop(1)) {

#pragma GCC diagnostic pop

        const auto this_seq_num = packet.get_sequence_number();
        const auto prev_seq_num = stream.back().get().get_sequence_number();

        if (const auto this_file_id = packet.get_file_id(); this_file_id != working_file_id)
            throw std::runtime_error(
                std::format("Refusing CTFS packet with File ID {} when expecting {}.", this_file_id, working_file_id));

        if (this_seq_num == prev_seq_num) {
            if (!std::ranges::equal(packet.observe_payload(), stream.back().get().observe_payload()))
                throw std::runtime_error(
                std::format("Refusing CTFS packet with File ID {} and duplicated Sequence Number {}.",
                            working_file_id,
                            this_seq_num));
        } else {
            if (this_seq_num != prev_seq_num + 1)
                throw std::runtime_error(
                    std::format("Refusing CTFS packet stream with File ID {} and missing Sequence Number {}.",
                                working_file_id,
                                prev_seq_num + 1));

            stream.emplace_back(packet);
        }
    }

    if (stream.empty())
        throw std::runtime_error("Packet stream was empty.");

    return stream;
}

void PDFWriter::validate_pdf_payload_stream(std::span<const std::reference_wrapper<const CTFSPacket>> packets)
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
