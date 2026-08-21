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
#include <ranges>

#include "CTFSPacket.hpp"
#include "Logger.hpp"
#include "TrafficDecoder.hpp"

namespace firestorm
{

void Driver::run()
{
    static constexpr std::uint16_t incoming_port = 5733;

    std::array<std::byte, 64 * 1024> storage{};
    TrafficDecoder filter;

    TCPConnection source{incoming_port};
    const auto callback = std::bind_front(&Driver::process_record_view, this);

    for (std::span<const std::byte> bytes = source.read(storage); !bytes.empty(); bytes = source.read(storage))
        filter.consume(bytes, callback);

    for (const auto& [file_id, packets] : packet_groups) {
        std::ofstream output_file(std::filesystem::current_path() /
                                      std::format("{}.pdf", static_cast<unsigned int>(file_id)),
                                  std::ios::binary | std::ios::out);
        output_file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        write_pdf(output_file, packets);
    }

    packet_groups.clear();
}

void Driver::process_record_view(const TrafficRecordView& record_view)
{
    std::visit(
        [this](const auto& decoded) {
            process_record(decoded);
        },
        record_view);
}

void Driver::process_record(const CTFSPacketView& ctfs_packet_view)
{
    const auto expected_cs = ctfs_packet_view.compute_checksum();

    if (expected_cs == ctfs_packet_view.received_checksum) {
        if (!packet_groups[ctfs_packet_view.file_id].emplace(ctfs_packet_view.make_packet()).second)
            throw std::invalid_argument("Duplicate CTFS packet sequence number.");
    } else
        LOG_F_INFO("Dropping CTFS packet (F#{}, S#{}) due to invalid checksum: expected {}, got {}.",
                   ctfs_packet_view.file_id,
                   ctfs_packet_view.sequence_number,
                   expected_cs,
                   ctfs_packet_view.received_checksum);
}

void Driver::process_record(const EthernetFrameView& ethernet_frame_view)
{
    frame_router.route(ethernet_frame_view);
}

void Driver::write_pdf(std::ofstream& output, const std::set<CTFSPacket>& packets)
{
    for (const auto& packet : packets) {
        const auto payload = packet.observe_payload();
        output.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
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
        driver.run();
        return 0;
    } catch (const std::exception& error) {
        std::println(std::cerr, "FIRE STORM failed: {}", error.what());
    }

    return 0;
}
