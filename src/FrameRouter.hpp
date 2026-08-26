/**
 * @file
 *
 * FrameRouter specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_FRAMEROUTER_HPP
#define FIRESTORM_FRAMEROUTER_HPP

#include <algorithm>

#include "AsyncFrameSink.hpp"
#include "EthernetFrameView.hpp"

namespace firestorm
{

/**
 * Routes decoded Ethernet frames according to a selected routing policy.
 *
 * In passthrough mode, every frame is forwarded to the primary TCP sink.
 *
 * In classified mode, the router examines the source and destination MAC addresses and the enclosed IP protocol:
 *  - Frames with a monitored destination MAC prefix are copied to the monitor sink.
 *  - Frames with a malicious source MAC prefix are dropped.
 *  - Other frames are forwarded to either the TCP or UDP sink according to their IP protocol.
 *
 * The router maintains counts of processed frames and frames identified as having a malicious source.
 *
 * Connections to output endpoints are established lazily by the corresponding LazyFrameSink.
 *
 * @todo update documentation for async.
 */
class FrameRouter
{
public:
    /**
     * Determines the routing behaviour for Ethernet frames.
     */
    enum class Policy
    {
        Passthrough, /**< All frames are forwarded to the TCP sink without classification. */
        Classified   /**< Applies MAC-address and IP-protocol routing rules. */
    };

    /**
     * Creates a FrameRouter using the specified Policy.
     *
     * Constructing the FrameRouter does not establish any output connections.
     *
     * @param policy Routing policy to apply.
     */
    explicit FrameRouter(Policy policy);

    /**
     * Routes an Ethernet frame.
     *
     * In passthrough mode, forwards @p frame to the primary TCP sink. In classified mode, applies the
     * monitored-destination, malicious-source, and IP-protocol rules described by FrameRouter.
     *
     * @param view Decoded non-owning view of the frame to route.
     *
     * @note Storage referenced by @p frame must remain valid for the duration of this call.
     */
    void route(const EthernetFrameView& view);
    void finish();

private:
    using MACAddressView = std::span<const std::byte, 3>; /**< Non-owning view of a three-byte MAC address prefix. */
    using MACPrefix = std::array<std::byte, 3>;           /**< Three-byte MAC address prefix. */

    /** Network address of the sinks. */
    static constexpr auto endpoint_addr = "127.0.0.1";

    static constexpr std::uint16_t tcp_port = 5744;     /**< Port number for @ref tcp_sink. */
    static constexpr std::uint16_t monitor_port = 5745; /**< Port number for @ref monitor_sink. */
    static constexpr std::uint16_t udp_port = 5746;     /**< Port number for @ref udp_sink. */

    /**
     * Tests whether a MAC address starts with any supplied prefix.
     *
     * @tparam Prefixes Input range whose elements are comparable with a three-byte MAC address prefix.
     *
     * @param address MAC address to inspect.
     * @param prefixes Prefixes against which to compare the address.
     *
     * @return Do the first three bytes of @p address an element of @p prefixes?
     */
    template <std::ranges::input_range Prefixes>
    [[nodiscard]] static constexpr bool has_any_prefix(const MACAddressView address, const Prefixes& prefixes)
    {
        return std::ranges::any_of(prefixes, [address](const auto& prefix) {
            return std::ranges::equal(address.first<3>(), prefix);
        });
    }

    /**
     * Tests whether a destination MAC address is monitored.
     *
     * @param destination Prefix of the destination MAC to inspect.
     * @return Is @p destination monitored?
     */
    [[nodiscard]] static constexpr bool has_monitored_destination(MACAddressView destination) noexcept;


    /**
     * Tests whether a source MAC address is known to be malicious.
     *
     * @param source Prefix of the source MAC to inspect.
     * @return Is @p source known to represent a malicious device?
     */
    [[nodiscard]] static constexpr bool has_malicious_source(MACAddressView source) noexcept;

    Policy policy;
    std::size_t frame_count = 0;
    std::size_t malicious_count = 0;

    AsyncFrameSink tcp_sink{endpoint_addr, tcp_port}; /**< Primary sink for passthrough and TCP-classified traffic. */
    AsyncFrameSink monitor_sink{endpoint_addr, monitor_port}; /**< Sink for copies of monitored traffic. */
    AsyncFrameSink udp_sink{endpoint_addr, udp_port};         /**< Sink for copies of UDP-classified traffic. */
};

} // namespace firestorm

#endif // FIRESTORM_FRAMEROUTER_HPP
