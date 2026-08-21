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

void FrameRouter::route(const EthernetFrameView& frame)
{
    ++frame_count;

    if (policy == Policy::Classified) {
        const auto monitored = has_monitored_destination(frame.dst_mac);
        const auto malicious = has_malicious_source(frame.src_mac);

        if (monitored)
            monitor_sink.send(frame.bytes);

        if (malicious) {
            // Drop malicious packets following monitoring.
            if (!monitored)
                ++malicious_count;
            return;
        }

        switch (frame.ip_protocol) {
        case IPPROTO_TCP:
            tcp_sink.send(frame.bytes);
            break;

        case IPPROTO_UDP:
            udp_sink.send(frame.bytes);
            break;

        default: ; // Unsupported protocol.
        }
    } else
        tcp_sink.send(frame.bytes);
}

void FrameRouter::print_summary() const
{
    if (policy == Policy::Passthrough) {
        LOG_F_INFO("Under the passthrough policy, {} frames have been forwarded.", frame_count);
        tcp_sink.print_summary();
    } else {
        LOG_F_INFO("Under the classifying policy, {} frames have been routed, of which {} were dropped for being "
                   "malicious. In particular:",
                   frame_count,
                   malicious_count);

        tcp_sink.print_summary();
        monitor_sink.print_summary();
        udp_sink.print_summary();
    }
}

void FrameRouter::reset_statistics() noexcept
{
    frame_count = 0;
    malicious_count = 0;

    tcp_sink.reset_statistics();
    monitor_sink.reset_statistics();
    udp_sink.reset_statistics();
}

} // namespace firestorm
