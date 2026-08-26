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
 * Asynchronously forwards Ethernet frames to the primary endpoint.
 *
 * This class implements the FIRE STORM passthrough routing policy. Every frame passed to @ref route is copied into
 * owned, immutable storage and queued for delivery to the TCP endpoint at @c 127.0.0.1:5744.
 *
 * Network deliver is performed by the an AsyncFrameSink worker. Consequently, returning from @ref route means that the
 * frame has been accepted for asynchronous delivery.
 *
 * Call @ref drain_and_report after submitting the final frame to wait for all queued deliveries, report delivery
 * statistics, and propagate any asynchronous worker failures.
 */
class PassthroughAsyncFrameRouter final : public IFrameRouter
{
public:
    /**
     * Copies and queues an Ethernet frame for delivery to the TCP endpoint.
     *
     * @param view Decoded non-owning view of the frame to route.
     *
     * @throws std::bad_alloc The frame could not be copied or queued.
     * @throws std::exception The sink rejected the frame or reported an asynchronous worker failure.
     */
    void route(const EthernetFrameView& view) override;

    /**
     * Waits for queued frames and reports delivery statistics.
     *
     * The frame counter is reset following a successful drain.
     *
     * @throws std::exception The sink reported an asynchronous worker failure.
     */
    void drain_and_report() override;

private:
    static constexpr auto endpoint_addr = "127.0.0.1"; /**< Network address of the sinks. */
    static constexpr std::uint16_t port = 5744;        /**< Port number for @ref tcp_sink. */

    std::size_t frame_count = 0;
    AsyncFrameSink tcp_sink{endpoint_addr, port}; /**< Primary sink for passthrough and TCP-classified traffic. */
};

} // namespace firestorm

#endif // FIRESTORM_PASSTHROUGHASYNCFRAMEROUTER_HPP
