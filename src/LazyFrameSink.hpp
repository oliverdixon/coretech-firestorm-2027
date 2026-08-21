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
 * reached, the frame is dropped and the failed connection is not retried by subsequent calls to @ref send.
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
     * On the first call, we attempt to establish a connection to the endpoint. If the attempt fails, the frame is
     * counted as dropped and subsequent calls drop frames without retrying the connection. Otherwise, the complete
     * contents of @p frame are written to the connection.
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
    std::string host;
    std::uint16_t port;

    bool attempted = false;
    std::size_t frames_sent = 0;
    std::size_t frames_dropped = 0;
    std::optional<TCPConnection> transport;
};

} // namespace firestorm

#endif // FIRESTORM_LAZYFRAMESINK_HPP
