/**
 * @file
 *
 * PassthroughAsyncFrameRouter specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_PASSTHROUGHASYNCFRAMEROUTER_HPP
#define FIRESTORM_PASSTHROUGHASYNCFRAMEROUTER_HPP

#include <algorithm>

#include "AsyncFrameSink.hpp"
#include "IFrameRouter.hpp"

namespace firestorm
{

/**
 * @todo Document.
 */
class PassthroughAsyncFrameRouter : public IFrameRouter
{
public:
    ~PassthroughAsyncFrameRouter() noexcept override = default;

    /**
     * Routes an Ethernet frame.
     *
     * In passthrough mode, forwards @p frame to the primary TCP sink. In classified mode, applies the
     * monitored-destination, malicious-source, and IP-protocol rules described by FrameRouter.
     *
     * @param view Decoded non-owning view of the frame to route.
     */
    void route(const EthernetFrameView& view) override;
    void drain_and_report() override;

private:
    static constexpr auto endpoint_addr = "127.0.0.1"; /**< Network address of the sinks. */
    static constexpr std::uint16_t port = 5744;        /**< Port number for @ref tcp_sink. */

    std::size_t frame_count = 0;
    std::size_t malicious_count = 0;

    AsyncFrameSink tcp_sink{endpoint_addr, port}; /**< Primary sink for passthrough and TCP-classified traffic. */
};

} // namespace firestorm

#endif // FIRESTORM_PASSTHROUGHASYNCFRAMEROUTER_HPP
