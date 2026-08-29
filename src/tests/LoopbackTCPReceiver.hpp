/**
 * @file
 *
 * LoopbackTCPReceiver specification
 *
 * @author Oliver Dixon
 * @date 2026-08-28
 */

#ifndef FIRESTORM_LOOPBACKTCPRECEIVER_HPP
#define FIRESTORM_LOOPBACKTCPRECEIVER_HPP

#include <cstddef>
#include <future>
#include <memory>
#include <span>
#include <stop_token>
#include <thread>
#include <vector>

namespace firestorm::test
{

/**
 * Receives a TCP byte stream on an ephemeral IPv4 loopback port.
 *
 * The listening socket is bound before construction completes. A background worker accepts the requested number of
 * connections and stores each connection's complete byte stream separately.
 */
class LoopbackTCPReceiver
{
public:
    /**
     * Starts a receiver on an ephemeral loopback port.
     *
     * The OS selects an available port. Construction returns after the listening socket has been successfully bound and
     * placed into the listening state, but does not wait for a peer to connect.
     *
     * @throws std::system_error The listening socket cannot be created due to a low-level error.
     * @throws std::bad_alloc Internal state could not be allocated.
     */
    LoopbackTCPReceiver();

    /**
     * Starts a receiver on a specified loopback port.
     *
     * @param requested_port The requested port number.
     *
     * @throws std::system_error The listening socket cannot be created due to a low-level error.
     * @throws std::bad_alloc Internal state could not be allocated.
     */
    explicit LoopbackTCPReceiver(std::uint16_t requested_port);

    /**
     * Stops the worker and closes all sockets.
     */
    ~LoopbackTCPReceiver() noexcept;

    /**
     * Waits for all expected connections to close and rejoins the worker.
     *
     * Worker failures are propagated through this function.
     *
     * @throws std::runtime_error A connection or input timed out.
     * @throws std::system_error A socket operation failed.
     * @throws std::exception A worker has failed.
     */
    void wait();

    /**
     * Gets the assigned port number from the TCPConnection.
     *
     * @return Assigned ephemeral port number.
     */
    [[nodiscard]] std::uint16_t get_assigned_port() const noexcept;

    /**
     * Attempts to observe the bytes received over TCP.
     *
     * Prior to observing bytes, @ref wait must be called to finish any in-progress transports. If this is not done,
     * this function will throw.
     *
     * @return An observing span over the bytes received over the TCPConnection.
     *
     * @throws std::logic_error The receiver is not ready to be observed.
     */
    [[nodiscard]] std::span<const std::byte> try_observe_received_bytes() const;

private:
    /**
     * Accepts and receives one TCP byte stream on the loopback interface.
     *
     * This function is the entry point for the receiver's background worker. It creates a listening TCP connection on
     * @p requested_port and publishes the assigned port through @p ready once the socket has been bound and placed into
     * the listening state.
     *
     * After accepting one connection, the function reads until the peer closes its sending side and appends the
     * complete byte stream to @ref received_bytes.
     *
     * A stop request can cancel the wait for an incoming connection. It does not interrupt a blocking read from an
     * already accepted connection; that read completes when the peer sends data or closes the connection.
     *
     * Exceptions are caught internally. A failure occurring before the listening socket is ready is published through
     * @p ready. A subsequent worker failure is stored in @ref worker_failure and later propagated by @ref wait.
     * Cancellation requested through @p stop_token is not reported as a worker failure.
     *
     * @param stop_token Token used to cancel the wait for a connection.
     * @param ready Promise through which the assigned port or a startup failure is reported to the constructing thread.
     * @param requested_port Loopback port to bind, or zero to request an ephemeral port.
     *
     * @note This function is intended to execute only on @ref worker.
     */
    void receive(const std::stop_token& stop_token,
                 std::shared_ptr<std::promise<std::uint16_t>> ready,
                 uint16_t requested_port) noexcept;

    std::uint16_t assigned_port = 0;       /**< Bound port in host byte order. */
    std::vector<std::byte> received_bytes; /**< Complete byte stream received from the peer. */

    std::exception_ptr worker_failure; /**< Propagated failure from the @ref worker. */
    std::jthread worker;               /**< Worker responsible for accepting and reading from the connection. */
    bool wait_completed = false;       /**< Has @ref wait completed without an exception? */
};

} // namespace firestorm::test

#endif // FIRESTORM_LOOPBACKTCPRECEIVER_HPP
