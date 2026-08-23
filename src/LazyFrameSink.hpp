/**
 * @file
 *
 * LazyFrameSink specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_LAZYFRAMESINK_HPP
#define FIRESTORM_LAZYFRAMESINK_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "EthernetFrameView.hpp"
#include "TCPConnection.hpp"
#include "ThreadSafeQueue.hpp"

namespace firestorm
{

/**
 * Lazily established destination for forwarded Ethernet frames.
 *
 * LazyFrameSink defers establishing its TCP connection until the first frame is sent. If the destination cannot be
 * reached, the connection is retried using an exponential backoff pattern in subsequent packets; i.e., if it fails for
 * the first time, it will try again on the next packet; if it fails of a second, it waits for two packets, then four,
 * then eight, etc.
 *
 * @note Constructing a sink performs no network I/O.
 * @warning This class is not thread-safe.
 */
class LazyFrameSink
{
public:
    /**
     * Creates a frame sink for a remote TCP endpoint.
     *
     * The endpoint is stored for later use; the connection is not established until the first call to @ref send.
     *
     * @param host Hostname or network address of the destination.
     * @param port TCP port of the destination.
     *
     * @throws std::bad_alloc Storing @p host fails.
     */
    LazyFrameSink(std::string_view host, std::uint16_t port);

    /**
     * Sends an Ethernet frame to the destination.
     *
     * On the first call, we attempt to establish a connection to the endpoint. If the destination cannot be reached,
     * the connection is retried using an exponential backoff pattern in subsequent packets; i.e., if it fails for the
     * first time, it will try again on the next packet; if it fails of a second, it waits for two packets, then four,
     * then eight, etc.
     *
     * @param frame Complete encoded Ethernet frame to send.
     */
    void send(const EthernetFrameView& frame);

    /**
     * Report the sink's current transmission statistics.
     *
     * Reports the destination and the current counts of successfully sent and dropped frames using Logger.
     */
    void print_summary() const;

    /**
     * Resets the transmission statistics.
     */
    void reset_statistics() noexcept;

private:
    using FrameQueue = ThreadSafeQueue<EthernetFrameView>;

    struct Statistics
    {
        Statistics() noexcept = default;

        std::uint32_t frames_sent = 0;    /**< Number of frames successfully sent and received by the sink. */
        std::uint32_t frames_dropped = 0; /**< Number of frames dropped due to no connection with the sink. */
    };

    struct ConnectionMetadata
    {
        ConnectionMetadata() noexcept = default;

        /** Number of packets to wait before reattempting to establish @ref transport. */
        std::uint32_t connection_cooloff = 0;

        /** Total number of failed connection attempts since the last @ref reset_statistics call. */
        std::uint32_t failed_connection_attempts = 0;
    };

    void run_worker(const std::stop_token& stop_token);

    void attempt_connection();

    std::string host;
    std::uint16_t port;

    std::atomic<Statistics> statistics;
    std::atomic<ConnectionMetadata> connection_metadata;

    FrameQueue despatch_queue;
    std::jthread worker;
    std::optional<TCPConnection> transport;
};

} // namespace firestorm

#endif // FIRESTORM_LAZYFRAMESINK_HPP
