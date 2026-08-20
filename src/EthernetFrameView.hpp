/**
 * @file
 *
 * EthernetFrameView specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_ETHERNETFRAMEVIEW_HPP
#define FIRESTORM_ETHERNETFRAMEVIEW_HPP

#include <cstdint>
#include <span>

namespace firestorm
{

/**
 * @todo Document
 */
struct EthernetFrameView
{
    std::span<const std::byte> bytes;

    std::span<const std::byte, 6> src_mac;
    std::span<const std::byte, 6> dst_mac;

    std::span<const std::byte> ip_header;
    std::span<const std::byte> ip_payload;
    std::uint8_t ip_protocol;
};

} // namespace firestorm

#endif // FIRESTORM_ETHERNETFRAMEVIEW_HPP
