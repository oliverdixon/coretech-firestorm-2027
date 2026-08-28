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

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "AsyncFrameSink.hpp"
#include "IFrameRouter.hpp"

namespace firestorm
{

/**
 * Asynchronously forwards Ethernet frames to the primary endpoint.
 *
 * This class implements the FIRE STORM passthrough routing policy. Every frame passed to @ref route is copied into
 * owned, immutable storage and queued for delivery (by default) to the TCP endpoint at @c 127.0.0.1:5744.
 *
 * Network deliver is performed by the an AsyncFrameSink worker. Consequently, returning from @ref route means that the
 * frame has been accepted for asynchronous delivery.
 *
 * Call @ref drain_and_report after submitting the final frame to wait for all queued deliveries, report delivery
 * statistics, and propagate any asynchronous worker failures.
 */
class PassthroughAsyncFrameRouter final : public IFrameRouter
{
    static constexpr auto default_host = "127.0.0.1";   /**< Default network address for @ref tcp_sink. */
    static constexpr std::uint16_t default_port = 5744; /**< Default port number for @ref tcp_sink. */

public:
    /**
     * Creates a new PassthroughAsyncFrameRouter for routing frames to the specified endpoint.
     *
     * @param host Network address of the sink.
     * @param port Port number for the sink.
     */
    explicit PassthroughAsyncFrameRouter(std::string_view host = default_host, std::uint16_t port = default_port);

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
    std::size_t frame_count = 0; /**< Total number of frames sent and received by the endpoint. */
    AsyncFrameSink tcp_sink;     /**< Primary sink for passthrough and TCP-classified traffic. */
};

} // namespace firestorm

#endif // FIRESTORM_PASSTHROUGHASYNCFRAMEROUTER_HPP
