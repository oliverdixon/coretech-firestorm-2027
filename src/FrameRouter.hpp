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

#include "EthernetFrameView.hpp"
#include "LazyFrameSink.hpp"

namespace firestorm
{

/**
 * @todo Document
 */
class FrameRouter
{
public:
    void route(const EthernetFrameView& frame);

private:
    using MACAddressView = std::span<const std::byte, 6>;
    using MACPrefix = std::array<std::byte, 3>;

    static constexpr auto endpoint_addr = "127.0.0.1";

    static constexpr std::uint16_t tcp_port = 5744;
    static constexpr std::uint16_t monitor_port = 5745;
    static constexpr std::uint16_t udp_port = 5746;

    template <std::ranges::input_range Prefixes>
    [[nodiscard]] static constexpr bool has_any_prefix(const MACAddressView address, const Prefixes& prefixes)
    {
        return std::ranges::any_of(prefixes, [address](const auto& prefix) {
            return std::ranges::equal(address.first<3>(), prefix);
        });
    }

    [[nodiscard]] static constexpr bool has_monitored_destination(MACAddressView destination) noexcept;
    [[nodiscard]] static constexpr bool has_malicious_source(MACAddressView source) noexcept;

    LazyFrameSink tcp_sink{endpoint_addr, tcp_port};
    LazyFrameSink monitor_sink{endpoint_addr, monitor_port};
    LazyFrameSink udp_sink{endpoint_addr, udp_port};
};

} // namespace firestorm

#endif // FIRESTORM_FRAMEROUTER_HPP
