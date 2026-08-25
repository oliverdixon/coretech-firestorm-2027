/**
 * @file
 *
 * Driver implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "Driver.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <ranges>

#include "CTFSPacket.hpp"
#include "Logger.hpp"
#include "PDFWriter.hpp"
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
    decoder.finish(callback);

    auto nonempty_groups = packet_groups | std::views::filter([](const auto& g) {
                               return !g.empty();
                           });

    for (auto& packet_group : nonempty_groups) {
        const auto file_id = packet_group[0].get_file_id();
        std::ranges::sort(packet_group);

        try {
            // Set up the destination file.
            const auto path = std::filesystem::current_path() / std::format("{}.pdf", static_cast<unsigned int>(file_id));
            std::ofstream output_file(path, std::ios::binary | std::ios::out);
            output_file.exceptions(std::ofstream::failbit | std::ofstream::badbit);

            // Serialise the packets to the file.
            PDFWriter::write_pdf(output_file, packet_group);
            LOG_F_INFO("Reconstructed {} packets with File ID {} into {}.", packet_group.size(), file_id, path.c_str());
        } catch (const std::exception& error) {
            LOG_F_WARNING("Could not reconstruct packets for File ID {} because {}", file_id, error.what());
        }

        packet_group.clear();
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

    if (should_add) {
        // Resize the groups to store the new packet slot vector.
        if (packet_groups.size() < ctfs_packet_view.file_id + 1)
            packet_groups.resize(ctfs_packet_view.file_id + 1);

        // Construct the packet and insert into the file's packet group.
        packet_groups[ctfs_packet_view.file_id].emplace_back(ctfs_packet_view.make_packet());
    }
}

void Driver::process_record(const EthernetFrameView& ethernet_frame_view)
{
    frame_router.route(ethernet_frame_view);
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
