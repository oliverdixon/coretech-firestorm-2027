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
#include <cstdint>
#include <span>

#include "AsyncFrameSink.hpp"
#include "IFrameRouter.hpp"

namespace firestorm
{

/**
 * @todo Document
 */
class ClassifiedAsyncFrameRouter : public IFrameRouter
{
public:
    void route(const EthernetFrameView& view) override;
    void drain_and_report() override;

private:
    using MACAddressView = std::span<const std::byte, 3>; /**< Non-owning view of a three-byte MAC address prefix. */
    using MACPrefix = std::array<std::byte, 3>;           /**< Three-byte MAC address prefix. */

    static constexpr auto endpoint_addr = "127.0.0.1";    /**< Network address of the sinks. */
    static constexpr std::uint16_t tcp_port = 5744;       /**< Port number for @ref tcp_sink. */
    static constexpr std::uint16_t monitor_port = 5745;   /**< Port number for @ref monitor_sink. */
    static constexpr std::uint16_t udp_port = 5746;       /**< Port number for @ref udp_sink. */

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

    std::size_t frame_count = 0;
    std::size_t malicious_count = 0;

    AsyncFrameSink tcp_sink{endpoint_addr, tcp_port}; /**< Primary sink for passthrough and TCP-classified traffic. */
    AsyncFrameSink monitor_sink{endpoint_addr, monitor_port}; /**< Sink for monitored traffic. */
    AsyncFrameSink udp_sink{endpoint_addr, udp_port};         /**< Sink for UDP-classified traffic. */
};

} // namespace firestorm

#endif // FIRESTORM_CLASSIFIEDASYNCFRAMEROUTER_HPP
