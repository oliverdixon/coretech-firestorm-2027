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

namespace firestorm
{

constexpr bool FrameRouter::has_monitored_destination(const MACAddressView destination) noexcept
{
    static constexpr std::array monitored_destinations{
        MACPrefix{ std::byte{0x00}, std::byte{0x1a}, std::byte{0xa0} },
        MACPrefix{ std::byte{0x00}, std::byte{0x1e}, std::byte{0xc9} },
        MACPrefix{ std::byte{0x00}, std::byte{0x15}, std::byte{0x5d} },
    };

    return has_any_prefix(destination, monitored_destinations);
}

constexpr bool FrameRouter::has_malicious_source(const MACAddressView source) noexcept
{
    static constexpr std::array malicious_sources{
        MACPrefix{ std::byte{0x00}, std::byte{0x0c}, std::byte{0x29} },
        MACPrefix{ std::byte{0x00}, std::byte{0x50}, std::byte{0x56} },
        MACPrefix{ std::byte{0x00}, std::byte{0x1c}, std::byte{0x14} },
    };

    return has_any_prefix(source, malicious_sources);
}

void FrameRouter::route(const EthernetFrameView& frame)
{
    const auto monitored = has_monitored_destination(frame.dst_mac);
    const auto malicious = has_malicious_source(frame.src_mac);

    if (monitored)
        monitor_sink.send(frame.bytes);

    if (malicious)
        // Drop malicious packets following monitoring.
        return;

    switch (frame.ip_protocol) {
    case IPPROTO_TCP:
        tcp_sink.send(frame.bytes);
        break;

    case IPPROTO_UDP:
        udp_sink.send(frame.bytes);
        break;

    default: // Unsupported protocol.
    }
}

} // namespace firestorm
