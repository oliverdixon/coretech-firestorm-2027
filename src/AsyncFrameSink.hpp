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
 * Sink for asynchronously transporting frames to a network endpoint.
 *
 * @todo Document.
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
     * @param maximum_buffered_bytes Maximum number of bytes to buffer, defaults to 8MiB.
     *
     * @throws std::bad_alloc Storing @p host fails.
     */
    AsyncFrameSink(std::string_view host, std::uint16_t port, std::size_t maximum_buffered_bytes = 8U * 1024U * 1024U);

    /**
     * Drains the sync and closes the connection.
     */
    ~AsyncFrameSink() noexcept;

    /**
     * Enqueues a new frame for asynchronous transport to the endpoint.
     *
     * @param frame The frame payload bytes.
     *
     * @throws std::invalid_argument @p frame was empty.
     * @throws std::length_error @p frame did not fit in the buffer.
     * @throws std::exception the worker reported an error on its previous operation.
     * @throws std::logic_error The sink is closing down and is not accepting new frames.
     */
    void enqueue(std::shared_ptr<const std::vector<std::byte>> frame);

    /**
     * Drains existing frames from the queue and rejoins the worker thread.
     *
     * Following the drain operation, the sink is considered closed will accept no more frames.
     *
     * @return A pair consisting of the number of successfully sent frames, followed by the number of dropped frames.
     *
     * @throws std::exception the worker reported an error on its previous operation.
     */
    std::pair<size_t, size_t> drain();

    AsyncFrameSink(const AsyncFrameSink&) = delete;
    AsyncFrameSink& operator=(const AsyncFrameSink&) = delete;
    AsyncFrameSink(AsyncFrameSink&&) = delete;
    AsyncFrameSink& operator=(AsyncFrameSink&&) = delete;

    /**
     * Retrieves a formatted string representing the endpoint.
     *
     * @return The endpoint address in <code>host:port</code> format.
     */
    [[nodiscard]] std::string endpoint() const;

private:

    /**
     * State shared across the main thread and the encapsulated worker.
     *
     * SharedState accesses should be protected by a synchronisation mechanism, such as a mutex.
     */
    struct SharedState
    {
        bool closing = false;              /**< Is the endpoint connection in progress of being closed? */
        std::exception_ptr worker_failure; /**< Propagated exception from the worker. */
        std::deque<std::shared_ptr<const std::vector<std::byte>>> frames; /**< Queue of shared-ownership frames. */
        std::size_t buffered_bytes = 0; /**< Total number of bytes present in @ref frames. */
    };

    /**
     * Metadata for the connection.
     *
     * This state may be shared between states, providing they run non-concurrently. Statistics is updated by the worker
     * thread and queried by the main thread following re-joining. This is a bit awkward, but avoids the use of
     * synchronisation primitives in hot loops.
     */
    struct Statistics
    {
        /** Number of packets to wait before reattempting to establish the connection. */
        std::size_t connection_cooloff = 0;

        std::size_t failed_connection_attempts = 0; /**< Total number of failed connection attempts. */
        std::size_t frames_sent = 0;                /**< Total number of frames sent to the endpoint. */
        std::size_t frames_dropped = 0;             /**< Total number of frames dropped due to unconnected endpoint. */
    };

    /**
     * Attempt to connect to the stored endpoint.
     *
     * If connection succeeds, @ref transport will hold the TCPConnection. Otherwise it remains in its state prior to
     * the call, and a cooling off period is applied. The cooling off period is exponential backoff in the number failed
     * connection attempts. Attempting a connection during the cooling off period will do nothing.
     */
    void attempt_connection();

    /**
     * Receives frames on the SharedState queue and forwards them to the endpoint.
     *
     * If the endpoint is unconnected, @ref attempt_connection is used to establish a connection. The frame is popped
     * from the queue; if connected, the frame is sent to the endpoint; otherwise, it is dropped. In all cases
     * Statistics is updated.
     *
     * All exceptions are captured into the SharedShared for propagation by the main thread. If an exception occurs, the
     * event loop stops and this function returns.
     */
    void run_worker() noexcept;

    const std::string host;                   /**< Network address of the endpoint. */
    const std::uint16_t port;                 /**< Port number of the endpoint. */
    const std::size_t maximum_buffered_bytes; /**< Maximum number of total bytes resident in the buffer. */

    Statistics statistics;                    /**< Connection metadata. */

    std::mutex shared_state_mutex;            /**< Synchronisation primitive to protect @ref shared_state. */
    std::condition_variable frame_available;  /**< Indicator for new frames present on the queue. */
    std::condition_variable buffer_available; /**< Indicator for the frame buffer being available for writing. */

    SharedState shared_state;                 /**< State shared between main thread and worker. */
    std::optional<TCPConnection> transport;   /**< Worker-controlled endpoint. */
    std::jthread worker;                      /**< Thread to manage TCP communication. */
};

} // namespace firestorm

#endif // FIRESTORM_ASYNCFRAMESINK_HPP
