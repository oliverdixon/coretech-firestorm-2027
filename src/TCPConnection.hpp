/**
 * @file
 *
 * TCPConnection specification
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#ifndef FIRESTORM_TCPCONNECTION_HPP
#define FIRESTORM_TCPCONNECTION_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stop_token>
#include <string_view>

namespace firestorm
{

/**
 * Owns a single TCP connection.
 *
 * TCPConnection provides exclusive RAII ownership of a POSIX socket. Construction either connects to a remote endpoint
 * or waits for an incoming connection on a specified port.
 */
class TCPConnection
{
public:
    /**
     * Connects to a remote TCP endpoint.
     *
     * Resolves @p host and attempts to connect to the first usable address associated with it. The constructor returns
     * once the connection has been established.
     *
     * @param host Hostname or textual IP address of the remote endpoint.
     * @param port TCP port of the remote endpoint, in host byte order.
     *
     * @throws std::runtime_error Address resolution failed.
     * @throws std::system_error An OS-level networking call failed.
     */
    TCPConnection(std::string_view host, std::uint16_t port);

    /**
     * Waits for and accepts one incoming TCP connection.
     *
     * Binds a temporary listening socket to all local IPv4 interfaces on @p port. The constructor blocks until a
     * peer connects, cleans up the listening socket, and retains ownership of the accepted connection.
     *
     * @param port Local TCP port on which to accept the connection, in host byte order.
     * @param on_listening An optional callback to invoke with the port number following a successful listen call.
     * @param stop_token A stop token to indicate cancellation of awaiting a client connection.
     *
     * @throws std::system_error An OS-level networking call failed.
     */
    explicit TCPConnection(std::uint16_t port,
                           const std::function<void(std::uint16_t)>& on_listening = {},
                           const std::stop_token& stop_token = {});

    /**
     * Closes the owned TCP connection.
     */
    ~TCPConnection() noexcept;

    TCPConnection(const TCPConnection&) = delete;
    TCPConnection& operator=(const TCPConnection&) = delete;

    TCPConnection(TCPConnection&& other) noexcept; /**< Move a TCPConnection. */
    TCPConnection& operator=(TCPConnection&& other) noexcept; /**< Move-assign a TCPConnection. */

    /**
     * Reads available bytes over the TCP socket into the given storage buffer.
     *
     * The maximum number of bytes read is given by the size of @p storage.
     *
     * @param storage Allocated destination buffer for received bytes.
     * @return A non-owning observing view of the output buffer.
     *
     * @throws std::invalid_argument @p storage had no capacity.
     * @throws std::system_error The OS-level network receive call failed.
     */
    [[nodiscard]] std::span<const std::byte> read(std::span<std::byte> storage) const;

    /**
     * Writes bytes to the connection.
     *
     * Performs successive send operations to transmit the entire @p data payload to the endpoint. An empty span results
     * in no operation.
     *
     * @param data Bytes to write to the connection.
     *
     * @throws std::system_error A send operation failed, or could only transmit an unexpected number of bytes.
     */
    void write_all(std::span<const std::byte> data) const;

private:
    /**
     * Writes bytes to the connection.
     *
     * Performs a single send operation. The number of bytes written may be smaller than @p data. If the send operation
     * is interrupted by a signal, it is automatically retried.
     *
     * @param data Bytes to write to the connection.
     * @return Number of bytes successfully written. Returns zero if @p data is empty.
     *
     * @throws std::system_error The send operation failed.
     */
    [[nodiscard]] std::size_t write(std::span<const std::byte> data) const;

    /**
     * Creates and connects a TCP socket.
     *
     * @param host Hostname or textual IP address of the remote endpoint.
     * @param port TCP port of the remote endpoint, in host byte order.
     *
     * @throws std::runtime_error Address resolution failed.
     * @throws std::system_error An OS-level networking call failed.
     *
     * @return File descriptor of the connected socket.
     */
    static int get_connection_fd(std::string_view host, std::uint16_t port);

    /**
     * Accepts one connection and returns its descriptor.
     *
     * Creates an ephemeral listening socket and awaits a connection from a client. Once the listening socket is ready,
     * the callback @p on_listening is invoked with the port number of the connection. While waiting for a client, the
     * @p stop_token is polled every 50ms; if a stop has been requested, an exception is thrown.
     *
     * @param port Local TCP port on which to accept the connection, in host byte order.
     * @param on_listening Callback to invoke with the port number following a successful listen call.
     * @param stop_token Token to cancel the operation.
     *
     * @throws std::system_error An OS-level networking call failed.
     * @throws std::system_error A stop was requested.
     *
     * @return File descriptor of the accepted connection socket.
     */
    static int get_accept_fd(std::uint16_t port,
                             const std::function<void(uint16_t)>& on_listening,
                             const std::stop_token& stop_token);

    /**
     * Attempts to connect to a client within a fixed polling period.
     *
     * @param listener Socket descriptor of the ephemeral listening socket.
     * @param stop_token Token to request cancellation of the blocking accept.
     *
     * @throws std::system_error An OS-level networking call failed.
     * @throws std::system_error A stop was requested.
     *
     * @return File descriptor of the accepted connection socket, or @c -1 if no client connected.
     */
    static int polling_accept(int listener, const std::stop_token& stop_token);

    int fd; /**< File descriptor of the active connected socket. */
};

} // namespace firestorm

#endif // FIRESTORM_TCPCONNECTION_HPP
