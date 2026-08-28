/**
 * @file
 *
 * TestPDFWriter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-27
 */

#include <span>
#include <string>

#include "../../CTFSPacket.hpp"
#include "../../PDFWriter.hpp"
#include "../TestHarness.hpp"

using namespace firestorm;
using namespace firestorm::test;

namespace
{

/**
 * Helper to serialise an ordered list of CTFS packets into a PDF document.
 *
 * @param packets The packets, ordered by sequence number, to serialise.
 * @return The serialised PDF binary data.
 */
std::string serialise_pdf(const std::span<const CTFSPacket> packets)
{
    std::ostringstream output{std::ios::out | std::ios::binary};
    PDFWriter::write_pdf(output, packets);
    return output.str();
}

/**
 * @test Verifies exact reconstruction of PDF binary data.
 *
 * @param harness Active harness.
 */
void write_single_packet_exactly(TestHarness& harness)
{
    std::string pdf{"%PDF-1.7\nstream\n"};
    pdf.push_back('\0');
    pdf.append("binary\nendstream\n%%EOF");

    std::vector<CTFSPacket> packets;
    packets.emplace_back(0, 0, std::as_bytes(std::span{pdf}));

    CHECK_EQUALS(serialise_pdf(packets), pdf);
}

/**
 * @test Verifies that reconstruction is independent of packet boundaries.
 *
 * Every possible division of the PDF into two packets is tested. This includes divisions with the PDF header and EOF
 * marker.
 *
 * @param harness Active harness.
 */
void write_across_packet_boundaries(TestHarness& harness)
{
    const std::string pdf = "%PDF-1.7\nbody\n%%EOF";

    for (std::size_t split = 0; split <= pdf.size(); ++split) {
        std::vector<CTFSPacket> packets;
        packets.emplace_back(0, 0, std::as_bytes(std::span{pdf}.first(split)));
        packets.emplace_back(1, 0, std::as_bytes(std::span{pdf}.subspan(split)));
        CHECK_EQUALS(serialise_pdf(packets), pdf);
    }
}

/**
 * @test Verifies that trailing whitespace is accepted and preserved by the PDFWriter.
 *
 * @param harness Active harness.
 */
void accept_trailing_padding(TestHarness& harness)
{
    std::string padding;
    padding.push_back('\0');
    padding.append(" \t\r\n\f");

    const std::string pdf = "%PDF-1.7\nbody\n%%EOF" + padding;

    std::vector<CTFSPacket> packets;
    packets.emplace_back(0, 0, std::as_bytes(std::span{pdf}));

    CHECK_EQUALS(serialise_pdf(packets), pdf);
}

/**
 * @test Verifies the 1024-byte EOF-search boundary.
 *
 * An EOF marker in the final portion of the 1024 search boundary should be accepted; anything beyond that should be
 * rejected with an exception.
 *
 * @param harness Active harness.
 */
void enforce_eof_search_limit(TestHarness& harness)
{
    const std::string eof_marker = "%%EOF";
    const std::string body = "%PDF-1.7\nbody\n%%EOF";

    const auto valid_pdf = body + std::string(1024 - eof_marker.size(), ' ');
    std::vector<CTFSPacket> valid_packets;
    valid_packets.emplace_back(0, 0, std::as_bytes(std::span{valid_pdf}));
    CHECK_EQUALS(serialise_pdf(valid_packets), valid_pdf);

    const auto invalid_pdf = body + std::string(1024 - eof_marker.size() + 1, ' ');
    std::vector<CTFSPacket> invalid_packets;
    invalid_packets.emplace_back(0, 0, std::as_bytes(std::span{invalid_pdf}));
    CHECK_THROWS(serialise_pdf(invalid_packets), std::runtime_error);
}

/**
 * @test Verifies that an empty input is rejected.
 *
 * @param harness Active harness.
 */
void reject_empty_input(TestHarness& harness)
{
    constexpr std::vector<CTFSPacket> packets;
    CHECK_THROWS(serialise_pdf(packets), std::runtime_error);
}

} // namespace

int main(const int argc, const char* const* const argv)
{
    // clang-format off

    return TestHarness::run(argc, argv, {
        {
            .id = "write_single_packet_exactly",
            .name = "PDF writer preserves binary data",
            .function = write_single_packet_exactly
        },
        {
            .id = "write_across_packet_boundaries",
            .name = "PDF serialisation is independent of packet boundaries",
            .function = write_across_packet_boundaries
        },
        {
            .id = "accept_trailing_padding",
            .name = "PDF writer accepts and preserves trailing whitespace",
            .function = accept_trailing_padding
        },
        {
            .id = "enforce_eof_search_limit",
            .name = "PDF writer enforces the 1024 search bound for trailing EOF",
            .function = enforce_eof_search_limit
        },
        {
            .id = "reject_empty_input",
            .name = "PDF writer rejects an empty packet collection",
            .function = reject_empty_input
        },
    });

    // clang-format on
}
