/**
 * @file
 *
 * Driver implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "Driver.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ranges>
#include <span>
#include <utility>
#include <variant>

#include "CTFSPacket.hpp"
#include "ClassifiedAsyncFrameRouter.hpp"
#include "IFrameRouter.hpp"
#include "Logger.hpp"
#include "PDFWriter.hpp"
#include "PassthroughAsyncFrameRouter.hpp"
#include "TCPConnection.hpp"
#include "TrafficDecoder.hpp"

namespace
{

template <class... Ts>
struct Overloads : Ts...
{
    using Ts::operator()...;
};

} // namespace

namespace firestorm
{

Driver::Driver(const bool classified_policy, const bool drop_bad_checksums)
    : drop_bad_checksums(drop_bad_checksums)
{
    if (classified_policy)
        frame_router = std::make_unique<ClassifiedAsyncFrameRouter>();
    else
        frame_router = std::make_unique<PassthroughAsyncFrameRouter>();
}

Driver::~Driver() noexcept = default;

void Driver::run()
{
    static constexpr std::uint16_t incoming_port = 5733;

    std::array<std::byte, std::size_t{64} * 1024> storage{};
    TrafficDecoder decoder(drop_bad_checksums);

    const TCPConnection source{incoming_port};
    const auto callback = std::bind_front(&Driver::despatch_record, this);

    for (std::span<const std::byte> bytes = source.read(storage); !bytes.empty(); bytes = source.read(storage))
        decoder.consume(bytes, callback);
    decoder.finish();

    auto nonempty_groups = packet_groups | std::views::filter([](const auto& g) {
                               return !g.empty();
                           });

    for (auto& packet_group : nonempty_groups) {
        const auto file_id = packet_group[0].get_file_id();
        std::ranges::sort(packet_group);

        try {
            // Set up the destination file.
            const auto path =
                std::filesystem::current_path() / std::format("{}.pdf", static_cast<unsigned int>(file_id));
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

    frame_router->drain_and_report();
    packet_groups.clear();
}

void Driver::despatch_record(TrafficRecord&& record)
{
    std::visit(
        [this]<typename RecordT>(RecordT&& decoded) {
            process_record(std::forward<RecordT>(decoded));
        },
        std::move(record));
}

void Driver::process_record(CTFSPacket&& ctfs_packet)
{
    const auto file_id = ctfs_packet.get_file_id();

    // Resize the groups to store the new packet slot vector.
    if (packet_groups.size() < file_id + 1)
        packet_groups.resize(file_id + 1);

    // Move the packet into the file's packet group.
    packet_groups[file_id].emplace_back(std::move(ctfs_packet));
}

// ReSharper disable once CppMemberFunctionMayBeConst
void Driver::process_record(const EthernetFrameView& ethernet_frame_view)
{
    frame_router->route(ethernet_frame_view);
}

} // namespace firestorm
