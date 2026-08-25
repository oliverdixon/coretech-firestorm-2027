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
#include <span>
#include <string>
#include <string_view>

#include "TCPConnection.hpp"

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
    void send(std::span<const std::byte> frame);

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

    std::string host;
    std::uint16_t port;

    /** Number of packets to wait before reattempting to establish @ref transport. */
    std::size_t connection_cooloff = 0;

    /** Total number of failed connection attempts since the last @ref reset_statistics call. */
    std::size_t failed_connection_attempts = 0;

    std::size_t frames_sent = 0;
    std::size_t frames_dropped = 0;
    std::optional<TCPConnection> transport;
};

} // namespace firestorm

#endif // FIRESTORM_LAZYFRAMESINK_HPP
