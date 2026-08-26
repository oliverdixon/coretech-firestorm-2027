/**
 * @file
 *
 * AsyncFrameRouter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "AsyncFrameRouter.hpp"

#include <netinet/in.h>

#include "Logger.hpp"

namespace firestorm
{

constexpr bool AsyncFrameRouter::has_monitored_destination(const MACAddressView destination) noexcept
{
    static constexpr std::array monitored_destinations{
        MACPrefix{std::byte{0x00}, std::byte{0x1a}, std::byte{0xa0}},
        MACPrefix{std::byte{0x00}, std::byte{0x1e}, std::byte{0xc9}},
        MACPrefix{std::byte{0x00}, std::byte{0x15}, std::byte{0x5d}},
    };

    return has_any_prefix(destination, monitored_destinations);
}

constexpr bool AsyncFrameRouter::has_malicious_source(const MACAddressView source) noexcept
{
    static constexpr std::array malicious_sources{
        MACPrefix{std::byte{0x00}, std::byte{0x0c}, std::byte{0x29}},
        MACPrefix{std::byte{0x00}, std::byte{0x50}, std::byte{0x56}},
        MACPrefix{std::byte{0x00}, std::byte{0x1c}, std::byte{0x14}},
    };

    return has_any_prefix(source, malicious_sources);
}

AsyncFrameRouter::AsyncFrameRouter(const Policy policy) :
    policy(policy)
{
}

void AsyncFrameRouter::route(const EthernetFrameView& view)
{
    ++frame_count;

    const auto classified = policy == Policy::Classified;
    const auto monitored = classified && has_monitored_destination(view.dst_mac_prefix);
    const auto malicious = classified && has_malicious_source(view.src_mac_prefix);

    AsyncFrameSink* primary_sink = nullptr; // Primary sink, if any, to which the frame should be routed.

    if (!classified)
        primary_sink = &tcp_sink;
    else if (!malicious)
        switch (view.ip_protocol) {
        case IPPROTO_TCP:
            primary_sink = &tcp_sink;
            break;
        case IPPROTO_UDP:
            primary_sink = &udp_sink;
            break;
        default: ;
        }
    else
        ++malicious_count;

    // If we don't have anywhere to route it, bail out now.
    if (!monitored && primary_sink == nullptr)
        return;

    // Otherwise, take a copy of the frame and forward it to the interested endpoints.
    auto frame = std::make_shared<const std::vector<std::byte>>(view.bytes.begin(), view.bytes.end());

    if (monitored)
        monitor_sink.enqueue(frame);

    if (primary_sink)
        primary_sink->enqueue(std::move(frame));
}

void AsyncFrameRouter::drain_and_report()
{
    if (policy == Policy::Passthrough) {
        const auto [sent, dropped] = tcp_sink.drain();
        LOG_F_INFO("Under the passthrough policy, {} frames have been forwarded.", frame_count);
        LOG_F_INFO("To the endpoint {}, {} frames were sent, and {} were dropped.", tcp_sink.endpoint(), sent, dropped);
    } else {
        LOG_F_INFO("Under the classifying policy, {} frames have been routed, of which {} were excluded from their "
                   "primary route.",
                   frame_count,
                   malicious_count);

        std::exception_ptr first_failure = nullptr;

        // Drain and capture the first error. Report endpoint statistics if possible.
        auto drain = [&first_failure](AsyncFrameSink& sink) noexcept {
            try {
                const auto [sent, dropped] = sink.drain();
                LOG_F_INFO("To the endpoint {}, {} frames were sent, and {} were dropped.",
                           sink.endpoint(),
                           sent,
                           dropped);
            } catch (...) {
                if (!first_failure)
                    first_failure = std::current_exception();
            }
        };

        drain(tcp_sink);
        drain(monitor_sink);
        drain(udp_sink);

        if (first_failure)
            std::rethrow_exception(first_failure);
    }

    frame_count = 0;
    malicious_count = 0;
}

} // namespace firestorm
