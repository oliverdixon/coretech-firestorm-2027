/**
 * @file
 *
 * FrameRouter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "FrameRouter.hpp"

#include <netinet/in.h>

#include "Logger.hpp"

namespace firestorm
{

constexpr bool FrameRouter::has_monitored_destination(const MACAddressView destination) noexcept
{
    static constexpr std::array monitored_destinations{
        MACPrefix{std::byte{0x00}, std::byte{0x1a}, std::byte{0xa0}},
        MACPrefix{std::byte{0x00}, std::byte{0x1e}, std::byte{0xc9}},
        MACPrefix{std::byte{0x00}, std::byte{0x15}, std::byte{0x5d}},
    };

    return has_any_prefix(destination, monitored_destinations);
}

constexpr bool FrameRouter::has_malicious_source(const MACAddressView source) noexcept
{
    static constexpr std::array malicious_sources{
        MACPrefix{std::byte{0x00}, std::byte{0x0c}, std::byte{0x29}},
        MACPrefix{std::byte{0x00}, std::byte{0x50}, std::byte{0x56}},
        MACPrefix{std::byte{0x00}, std::byte{0x1c}, std::byte{0x14}},
    };

    return has_any_prefix(source, malicious_sources);
}

FrameRouter::FrameRouter(const Policy policy) :
    policy(policy)
{
}

void FrameRouter::route(const EthernetFrameView& view)
{
    ++frame_count;

    const auto monitored = has_monitored_destination(view.dst_mac_prefix);
    const auto malicious = has_malicious_source(view.src_mac_prefix);

    AsyncFrameSink * ordinary = nullptr;

    if (policy == Policy::Passthrough)
        ordinary = &tcp_sink;
    else if (!malicious)
        switch (view.ip_protocol) {
        case IPPROTO_TCP:
            ordinary = &tcp_sink;
            break;
        case IPPROTO_UDP:
            ordinary = &udp_sink;
            break;
        default: ;
        }
    else
        ++malicious_count;

    if (!monitored && ordinary == nullptr)
        return;

    auto frame = std::make_shared<const std::vector<std::byte>>(view.bytes.begin(), view.bytes.end());
    if (monitored)
        monitor_sink.enqueue(frame);

    if (ordinary)
        ordinary->enqueue(std::move(frame));
}

void FrameRouter::finish()
{
    std::exception_ptr first_failure;

    auto finish_sink = [&first_failure](AsyncFrameSink& sink) {
        try {
            sink.drain();
        } catch (...) {
            if (!first_failure)
                first_failure = std::current_exception();
        }
    };

    finish_sink(tcp_sink);
    finish_sink(monitor_sink);
    finish_sink(udp_sink);

    if (first_failure)
        std::rethrow_exception(first_failure);
}

} // namespace firestorm
