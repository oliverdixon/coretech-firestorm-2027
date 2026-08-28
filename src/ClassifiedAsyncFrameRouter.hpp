/**
 * @file
 *
 * ClassifiedAsyncFrameRouter specification
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#ifndef FIRESTORM_CLASSIFIEDASYNCFRAMEROUTER_HPP
#define FIRESTORM_CLASSIFIEDASYNCFRAMEROUTER_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string_view>

#include "AsyncFrameSink.hpp"
#include "IFrameRouter.hpp"

namespace firestorm
{

/**
 * Asynchronously classifies and routes Ethernet frames.
 *
 * This class implements the FIRE STORM classified routing policy. The endpoints shown are defaults and may be
 * configured with constructor injection:
 *
 *  - A frame addressed to a monitored MAC prefix is sent to the monitoring endpoint at @c 127.0.0.1:5745.
 *  - A non-malicious TCP frame is sent to the TCP endpoint at @c 127.0.0.1:5744.
 *  - A non-malicious UDP frame is sent to the TCP endpoint at @c 127.0.0.1:5746.
 *  - A frame with neither a monitoring route nor a primary protocol route is discarded.
 *
 * Monitoring is independent of primary routing. A monitored frame from a malicious source is still sent to the
 * monitoring endpoint, while a monitored non-malicious TCP or UDP is sent to both applicable endpoints.
 *
 * Frames selected for delivery are copied into owned immutable storage. When two endpoints require the same frame,
 * their sinks share ownership of a single copy. Each AsyncFrameSink performs network delivery independently on its own
 * thread.
 *
 * Call @ref drain_and_report after submitting the final frame to wait for all three sinks, report per-endpoint
 * statistics, and propagate the first asynchronous worker failure. Every sink is given an opportunity to drain.
 */
class ClassifiedAsyncFrameRouter final : public IFrameRouter
{
    static constexpr auto default_endpoint_addr = "127.0.0.1";  /**< Default network address of the sinks. */
    static constexpr std::uint16_t default_tcp_port = 5744;     /**< Default port number for @ref tcp_sink. */
    static constexpr std::uint16_t default_monitor_port = 5745; /**< Default port number for @ref monitor_sink. */
    static constexpr std::uint16_t default_udp_port = 5746;     /**< Default port number for @ref udp_sink. */

public:
    /**
     * Creates a new ClassifiedAsyncFrameRouter for routing frames to the specified endpoints.
     *
     * @param host Network address of the sink endpoints.
     * @param tcp_port Port number for the TCP sink.
     * @param monitor_port Port number for the monitoring sink.
     * @param udp_port Port number for the UDP sink.
     */
    explicit ClassifiedAsyncFrameRouter(std::string_view host = default_endpoint_addr,
                                        std::uint16_t tcp_port = default_tcp_port,
                                        std::uint16_t monitor_port = default_monitor_port,
                                        std::uint16_t udp_port = default_udp_port);

    /**
     * Classifies an Ethernet frame and queues it for its applicable destinations.
     *
     * The @p view payload is copied only if at least one routing rule selects a destination. Where two destinations are
     * selected, both sinks share ownership of the same immutable copy.
     *
     * @param view Non-owning view of the decoded Ethernet frame.
     *
     * @throws std::bad_alloc The frame could not be copied or queued.
     * @throws std::exception A sink rejected the frame or reported an asynchronous worker failure.
     */
    void route(const EthernetFrameView& view) override;

    /**
     * Waits for every sink and reports routing statistics.
     *
     * All sinks are drained even if one reports an error. After draining, the first captured error is rethrown. Routing
     * counters are reset only when every sink drains successfully.
     *
     * @throws std::exception One or more sinks reported an asynchronous worker failure.
     */
    void drain_and_report() override;

private:
    using MACAddressView = std::span<const std::byte, 3>; /**< Non-owning view of a three-byte MAC address prefix. */
    using MACPrefix = std::array<std::byte, 3>;           /**< Three-byte MAC address prefix. */

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

    std::size_t frame_count = 0;     /**< Total number of framed routed into a sink. */
    std::size_t malicious_count = 0; /**< Total number of frames marked as malicious. */

    AsyncFrameSink tcp_sink;     /**< Primary sink for passthrough and TCP-classified traffic. */
    AsyncFrameSink monitor_sink; /**< Sink for monitored traffic. */
    AsyncFrameSink udp_sink;     /**< Sink for UDP-classified traffic. */
};

} // namespace firestorm

#endif // FIRESTORM_CLASSIFIEDASYNCFRAMEROUTER_HPP
