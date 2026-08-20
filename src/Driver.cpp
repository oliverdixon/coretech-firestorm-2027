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
#include <print>

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

    for (std::span<const std::byte> bytes = incoming.read(storage); !bytes.empty(); bytes = incoming.read(storage))
        filter.consume(bytes, forward_callback, capture_callback);

    std::println(std::cout, "Captured {} packets.\nForwarded {} bytes.", packets.size(), forwarded_bytes_count);
    write_pdf(pdf_destination);
}

void Driver::forward_bytes(std::span<const std::byte> bytes)
{
    std::println(std::cout, "Forwarding {} bytes.", bytes.size());
    forwarded_bytes_count += bytes.size();
}

void Driver::capture_packet(CTFSPacket&& packet)
{
    std::println(std::cout,
                 "Capturing packet #{} of {} bytes.",
                 packet.get_sequence_number(),
                 packet.observe_payload().size());

    packets.insert(std::move(packet));
}

void Driver::write_pdf(const std::filesystem::path& pdf_destination) const
{
    std::ofstream out(pdf_destination, std::ios::binary | std::ios::out);
    for (const auto& packet : packets) {
        const auto payload = packet.observe_payload();
        out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    }
}

} // namespace firestorm
