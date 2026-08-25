/**
 * @file
 *
 * AsyncFrameSink specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_ASYNCFRAMESINK_HPP
#define FIRESTORM_ASYNCFRAMESINK_HPP

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "TCPConnection.hpp"

namespace firestorm
{

/**
 * Lazily established destination for forwarded Ethernet frames.
 *
 * AsyncFrameSink defers establishing its TCP connection until the first frame is sent. If the destination cannot be
 * reached, the connection is retried using an exponential backoff pattern in subsequent packets; i.e., if it fails for
 * the first time, it will try again on the next packet; if it fails of a second, it waits for two packets, then four,
 * then eight, etc.
 *
 * @note Constructing a sink performs no network I/O.
 * @warning This class is not thread-safe.
 *
 * @todo Update documentation for async.
 */
class AsyncFrameSink
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
    AsyncFrameSink(std::string_view host, std::uint16_t port, std::size_t maximum_buffered_bytes = 8U * 1024U * 1024U);

    ~AsyncFrameSink() noexcept;

    void enqueue(std::shared_ptr<const std::vector<std::byte>> frame);
    void finish();

    AsyncFrameSink(const AsyncFrameSink&) = delete;
    AsyncFrameSink& operator=(const AsyncFrameSink&) = delete;
    AsyncFrameSink(AsyncFrameSink&&) = delete;
    AsyncFrameSink& operator=(AsyncFrameSink&&) = delete;

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
    /**
     * Attempt to connect to the stored endpoint.
     *
     * If connection succeeds, @ref transport will hold the TCPConnection. Otherwise it remains in its state prior to
     * the call, and a cooling off period is applied. The cooling off period is exponential backoff in the number failed
     * connection attempts. Attempting a connection during the cooling off period will do nothing.
     */
    void attempt_connection();

    void run_worker() noexcept;

    std::string host;
    std::uint16_t port;
    std::size_t maximum_buffered_bytes;
    std::size_t buffered_bytes = 0;

    /** Number of packets to wait before reattempting to establish @ref transport. */
    std::size_t connection_cooloff = 0;

    /** Total number of failed connection attempts since the last @ref reset_statistics call. */
    std::size_t failed_connection_attempts = 0;

    std::atomic_size_t frames_sent = 0;
    std::atomic_size_t frames_dropped = 0;

    std::mutex mutex;
    std::condition_variable frame_available; /**< Indicator for when a frame is available on the queue. */
    std::condition_variable buffer_available; /**< Indicator for  */
    std::deque<std::shared_ptr<const std::vector<std::byte>>> frame_queue;

    bool closing = false;
    std::exception_ptr worker_failure;
    std::optional<TCPConnection> transport;
    std::jthread worker;
};

} // namespace firestorm

#endif // FIRESTORM_ASYNCFRAMESINK_HPP
