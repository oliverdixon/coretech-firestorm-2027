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
     * @param output Output destination.
     * @param packets Ordered set of packets required to construct a PDF file at @p output.
     *
     * @throws std::ios_base::failure The entire PDF could not be reconstructed from @p packets.
     * @throws std::runtime_error The assembled byte stream was empty.
     * @throws std::runtime_error The assembled byte stream does not begin with the PDF magic bytes.
     * @throws std::runtime_error The assembled byte stream does not end with the PDF EOF marker.
     */
    static void write_pdf(std::ostream& output, std::span<const CTFSPacket> packets);

private:
    /**
     * Verifies that a sequence of packets contains a PDF byte stream.
     *
     * Performs a lightweight PDF validation by checking for the PDF magic byte sequences. In the final 1024 bytes, the
     * PDF EOF sequence is expected to occur at the end, except any trailing whitespace.
     *
     * @param packets Ordered packets comprising the PDF byte stream.
     *
     * @throws std::runtime_error The assembled byte stream was empty.
     * @throws std::runtime_error The assembled byte stream does not begin with the PDF magic bytes.
     * @throws std::runtime_error The assembled byte stream does not end with the PDF EOF marker.
     *
     * @note This is a smoke test only; it does not validate the internal PDF structure.
     */
    static void validate_pdf_stream(std::span<const CTFSPacket> packets);
};

} // namespace firestorm

#endif // FIRESTORM_PDFWRITER_HPP
