/**
 * @file
 *
 * Driver implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "Driver.hpp"

#include <fstream>
#include <functional>
#include <iostream>

#include "CTFSPacket.hpp"
#include "PacketFilter.hpp"

namespace firestorm
{

void Driver::run(const std::filesystem::path& pdf_destination)
{
    std::array<std::byte, 64 * 1024> storage{};
    PacketFilter filter;

    const auto forward_callback = std::bind_front(&Driver::forward_bytes, this);
    const auto capture_callback = std::bind_front(&Driver::capture_packet, this);

    const TCPConnection incoming{incoming_port};

    for (std::span<const std::byte> bytes = incoming.read(storage); !bytes.empty(); bytes = incoming.read(storage))
        filter.consume(bytes, forward_callback, capture_callback);
    filter.finish(forward_callback);

    write_pdf(pdf_destination);
    packets.clear();
}

void Driver::forward_bytes(const std::span<const std::byte> bytes)
{
    if (!outgoing.has_value())
        outgoing.emplace(endpoint_addr, outgoing_port);

    outgoing->write_all(bytes);
}

void Driver::capture_packet(CTFSPacket&& packet)
{
    if (!packets.insert(std::move(packet)).second)
        throw std::invalid_argument("Duplicate CTFS packet sequence number.");
}

void Driver::write_pdf(const std::filesystem::path& pdf_destination) const
{
    std::ofstream file(pdf_destination, std::ios::binary | std::ios::out);
    file.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    for (const auto& packet : packets) {
        const auto payload = packet.observe_payload();
        file.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    }
}

} // namespace firestorm

/**
 * FIRE STORM entry point.
 *
 * Runs FIRE STORM with its configured endpoints and output destination.
 *
 * @return Zero on success; nonzero if networking, parsing, or output fails.
 */
int main()
{
    using namespace firestorm;

    try {
        Driver driver;
        driver.run({"./file.pdf"});
        return 0;
    } catch (const std::exception& error) {
        std::println(std::cerr, "FIRE STORM failed: {}", error.what());
    }

    return 0;
}
