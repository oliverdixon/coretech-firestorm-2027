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

/**
 * @test Verifies that a byte stream with an invalid PDF header is rejected.
 *
 * @param harness Active harness.
 */
void reject_invalid_header(TestHarness& harness)
{
    const std::string invalid_pdf = "not a pdf\n%%EOF";

    std::vector<CTFSPacket> packets;
    packets.emplace_back(0, 0, std::as_bytes(std::span{invalid_pdf}));

    CHECK_THROWS(serialise_pdf(packets), std::runtime_error);
}

/**
 * @test Verifies that a byte stream with an invalid PDF EOF tail is rejected.
 *
 * @param harness Active harness.
 */
void reject_invalid_tail(TestHarness& harness)
{
    const std::string no_eof = "%PDF-1.7\nnot a pdf";
    const std::string corrupted_eof = "%PDF-1.7\nnot a pdf\n%%EOF\nbad data";

    std::vector<CTFSPacket> no_eof_packets;
    no_eof_packets.emplace_back(0, 0, std::as_bytes(std::span{no_eof}));
    CHECK_THROWS(serialise_pdf(no_eof_packets), std::runtime_error);

    std::vector<CTFSPacket> corrupted_eof_packets;
    corrupted_eof_packets.emplace_back(0, 0, std::as_bytes(std::span{corrupted_eof}));
    CHECK_THROWS(serialise_pdf(corrupted_eof_packets), std::runtime_error);
}

/**
 * @test Verifies that the PDFWriter ignores duplicated packets.
 *
 * @param harness Active harness.
 */
void ignore_duplicate_packets(TestHarness& harness)
{
    const std::string pdf = "%PDF-1.7\ndocument\n%%EOF";

    std::vector<CTFSPacket> packets;
    packets.emplace_back(0, 0, std::as_bytes(std::span{pdf}));
    packets.emplace_back(0, 0, std::as_bytes(std::span{pdf}));

    CHECK_EQUALS(serialise_pdf(packets), pdf);
}

/**
 * @test Verifies that the PDFWriter propagates stream write failures.
 *
 * @param harness Active harness.
 */
void propagate_output_failures(TestHarness& harness)
{
    /*
     * To simulate a failing buffer, we implement a std::streambuf that rejects any write. Thankfully std::streambuf is
     * a base class for character buffers with virtual member functions that makes this trivial.
     *
     * In particular, @c xsputn will always report that zero characters were written, and @c overflow always indicates
     * failure to write a character.
     */

    class RejectingBuffer final : public std::streambuf
    {
    protected:
        std::streamsize xsputn([[maybe_unused]] const char_type* const string,
                               [[maybe_unused]] const std::streamsize size) override
        {
            return 0;
        }

        int_type overflow([[maybe_unused]] const int_type ch) override
        {
            return traits_type::eof();
        }
    } rejecting_buffer;

    std::ostream rejecting_stream{&rejecting_buffer};
    const std::string pdf = "%PDF-1.7\ndocument\n%%EOF";

    std::vector<CTFSPacket> packets;
    packets.emplace_back(0, 0, std::as_bytes(std::span{pdf}));

    CHECK_THROWS(PDFWriter::write_pdf(rejecting_stream, packets), std::ios_base::failure);
}

} // namespace

/**
 * Executes selected PDFWriter tests with a TestHarness.
 *
 * For the meanings of @p argc and @p argv, see TestHarness::run documentation.
 *
 * @param argc Argument count
 * @param argv Argument vector
 *
 * @return Zero if all executed tests passed; non-zero otherwise.
 */
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
        {
            .id = "reject_invalid_header",
            .name = "PDF writer rejects a byte stream with an invalid PDF header",
            .function = reject_invalid_header
        },
        {
            .id = "reject_invalid_tail",
            .name = "PDF writer rejects a byte stream with an invalid PDF EOF tail",
            .function = reject_invalid_tail
        },
        {
            .id = "ignore_duplicate_packets",
            .name = "PDF writer ignores duplicated packets",
            .function = ignore_duplicate_packets
        },
        {
            .id = "propagate_output_failures",
            .name = "PDF writer propagates stream writer failures",
            .function = propagate_output_failures
        },
    });

    // clang-format on
}
