/**
 * @file
 *
 * FIRE STORM entry point
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>

#include "CTFSPacket.hpp"
#include "PacketFilter.hpp"
#include "TCPConnection.hpp"

int main()
{
    using namespace firestorm;

    TCPConnection incoming(5733);

    std::println(std::cout, "CONNECTED");

    std::array<std::byte, 64 * 1024> storage{};
    PacketFilter filter;
    std::set<CTFSPacket> packets;

    const auto forward_callback = [](const std::span<const std::byte> bytes) {
        std::println(std::cout, "Forwarding {} bytes.", bytes.size());
    };

    const auto capture_callback = [&packets](CTFSPacket&& packet) {
        packets.insert(std::move(packet));
    };

    for (std::span<const std::byte> bytes = incoming.read(storage); !bytes.empty(); bytes = incoming.read(storage))
        filter.consume(bytes, forward_callback, capture_callback);

    std::println(std::cout, "Captured {} packets.", packets.size());

    std::ofstream out("file.pdf", std::ios::binary | std::ios::out);
    for (const auto& packet : packets)
        out.write(reinterpret_cast<const char*>(packet.payload.data()),
                  static_cast<std::streamsize>(packet.payload.size()));

    return 0;
}
