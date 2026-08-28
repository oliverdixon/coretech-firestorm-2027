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
 *
 * @todo Document
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

    [[nodiscard]] std::span<const std::byte> observe_received_bytes() const;

private:
    // TODO document.
    void receive(const std::stop_token& stop_token,
                 std::shared_ptr<std::promise<std::uint16_t>> ready,
                 uint16_t requested_port);

    std::uint16_t assigned_port = 0;       /**< Bound port in host byte order. */
    std::vector<std::byte> received_bytes; /**< Complete byte stream received from the peer. */

    std::exception_ptr worker_failure;     /**< Propagated failure from the @ref worker. */
    std::jthread worker;                   /**< Worker responsible for accepting and reading from the connection. */
    bool wait_completed = false;           /**< Has @ref wait completed without an exception? */
};

} // namespace firestorm::test

#endif // FIRESTORM_LOOPBACKTCPRECEIVER_HPP
