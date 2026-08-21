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
#include <ranges>

#include "CTFSPacket.hpp"
#include "Logger.hpp"
#include "TrafficDecoder.hpp"

namespace firestorm
{

Driver::Driver(const FrameRouter::Policy routing_policy, const bool drop_bad_checksums) :
    drop_bad_checksums(drop_bad_checksums),
    frame_router(routing_policy)
{
}

void Driver::run()
{
    static constexpr std::uint16_t incoming_port = 5733;

    std::array<std::byte, 64 * 1024> storage{};
    TrafficDecoder decoder;

    TCPConnection source{incoming_port};
    const auto callback = std::bind_front(&Driver::process_record_view, this);

    for (std::span<const std::byte> bytes = source.read(storage); !bytes.empty(); bytes = source.read(storage))
        decoder.consume(bytes, callback);

    for (const auto& [file_id, packets] : packet_groups) {
        const auto path = std::filesystem::current_path() / std::format("{}.pdf", static_cast<unsigned int>(file_id));
        std::ofstream output_file(path, std::ios::binary | std::ios::out);
        output_file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        write_pdf(output_file, packets);
        LOG_F_INFO("Reconstructed {} packets with file ID {} into {}.", packets.size(), file_id, path.c_str());
    }

    frame_router.print_summary();
    frame_router.reset_statistics();

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
    bool should_add = false;

    if (expected_cs == ctfs_packet_view.received_checksum)
        should_add = true;
    else {
        if (drop_bad_checksums)
            LOG_F_WARNING("Dropping CTFS packet (F#{}, S#{}) due to invalid checksum: expected {:#X}, got {:#X}.",
                          ctfs_packet_view.file_id,
                          ctfs_packet_view.sequence_number,
                          expected_cs,
                          ctfs_packet_view.received_checksum);
        else {
            LOG_F_WARNING("Found CTFS packet (F#{}, S#{}) with bad checksum: expected {:#X}, got {:#X}.",
                          ctfs_packet_view.file_id,
                          ctfs_packet_view.sequence_number,
                          expected_cs,
                          ctfs_packet_view.received_checksum);
            should_add = true;
        }
    }

    if (should_add && !packet_groups[ctfs_packet_view.file_id].emplace(ctfs_packet_view.make_packet()).second)
        LOG_F_WARNING("Dropping CTFS packet with File ID {} and duplicated Sequence Number {}.",
                      ctfs_packet_view.file_id,
                      ctfs_packet_view.sequence_number);
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
 * Usage: `./FireStorm [--classified] [--drop-bad-checksums]`
 *
 * In classified mode, the application will follow the MAC-based routing rules. Otherwise, all non-operational data is
 * forwarded to the primary TCP endpoint. Disabled by default.
 *
 * If bad checksums are dropped, packets reporting invalid checksums will be ignored. Disabled by default.
 *
 * @param argc Number of arguments passed on the command line.
 * @param argv Argument vector.
 *
 * @return Zero on success; nonzero if networking, parsing, or output fails.
 */
int main(const int argc, const char* const* const argv)
{
    using namespace firestorm;

    // Process arguments.

    struct Options
    {
        bool drop_bad_checksums = false;
        FrameRouter::Policy policy = FrameRouter::Policy::Passthrough;
    } options;

    const auto args =
        std::span{argv, static_cast<std::size_t>(argc)} | std::views::transform([](const char* const arg) {
            return std::string_view{arg};
        });

    for (const auto arg : args)
        if (arg == "--drop-bad-checksums")
            options.drop_bad_checksums = true;
        else if (arg == "--classified")
            options.policy = FrameRouter::Policy::Classified;

    // Run the driver and report fatal errors.

    try {
        Driver driver(options.policy, options.drop_bad_checksums);
        driver.run();
        return 0;
    } catch (const std::exception& error) {
        LOG_F_WARNING("FIRE STORM failed: {}", error.what());
    }

    return 1;
}
