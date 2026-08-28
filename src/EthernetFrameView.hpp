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
 * Provides a non-owning decoded view of an Ethernet frame.
 *
 * The view exposes both the complete frame together with the IP protocol specifier, IPv4 header and payload, and source
 * and destination MAC addresses. The referenced byte storage must remain valid and unchanged for the lifetime of the
 * object.
 *
 * @note This type does not own the storage referenced by the span members.
 */
struct EthernetFrameView
{
    /** Complete encoded Ethernet frame including its header and payload. */
    std::span<const std::byte> bytes;

    std::span<const std::byte, 3> src_mac_prefix; /**< Three-byte prefix of the source MAC. */
    std::span<const std::byte, 3> dst_mac_prefix; /**< Three-byte prefix of the destination MAC. */

    std::span<const std::byte> ip_header;  /**< Encoded IPv4 header. */
    std::span<const std::byte> ip_payload; /**< Encoded IPv4 payload. */
    std::uint8_t ip_protocol;              /**< Encoded IP protocol (TCP or UDP). */
};

} // namespace firestorm

#endif // FIRESTORM_ETHERNETFRAMEVIEW_HPP
