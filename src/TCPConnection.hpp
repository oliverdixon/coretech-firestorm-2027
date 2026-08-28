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

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace firestorm
{

/**
 * Owns a single blocking TCP connection.
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
     *
     * @throws std::system_error An OS-level networking call failed.
     */
    explicit TCPConnection(std::uint16_t port, const std::function<void(std::uint16_t)>& on_listening = {});

    /**
     * Closes the owned TCP connection.
     */
    ~TCPConnection() noexcept;

    TCPConnection(const TCPConnection&) = delete;
    TCPConnection& operator=(const TCPConnection&) = delete;

    TCPConnection(TCPConnection&& other) noexcept;
    TCPConnection& operator=(TCPConnection&& other) noexcept;

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
     * Accepts one connection on an ephemeral listening socket.
     *
     * @param port Local TCP port on which to accept the connection, in host byte order.
     * @param on_listening Callback to invoke with the port number following a successful listen call.
     *
     * @throws std::system_error An OS-level networking call failed.
     *
     * @return File descriptor of the accepted connection socket.
     */
    static int get_accept_fd(std::uint16_t port, const std::function<void(uint16_t)>& on_listening);

    int fd; /**< File descriptor of the active socket. */
};

} // namespace firestorm

#endif // FIRESTORM_TCPCONNECTION_HPP
