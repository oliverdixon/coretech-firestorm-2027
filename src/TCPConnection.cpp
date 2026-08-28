/**
 * @file
 *
 * TCPConnection implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "TCPConnection.hpp"

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace firestorm
{

TCPConnection::TCPConnection(const std::string_view host, const std::uint16_t port)
    : fd(get_connection_fd(host, port))
{
}

TCPConnection::TCPConnection(const std::uint16_t port,
                             const std::function<void(std::uint16_t)>& on_listening,
                             const std::stop_token& stop_token)
    : fd(get_accept_fd(port, on_listening, stop_token))
{
}

TCPConnection::~TCPConnection() noexcept
{
    if (fd != -1)
        close(fd);
}

TCPConnection::TCPConnection(TCPConnection&& other) noexcept
    : fd(std::exchange(other.fd, -1))
{
}

TCPConnection& TCPConnection::operator=(TCPConnection&& other) noexcept
{
    if (this != &other) {
        if (fd != -1)
            close(fd);

        fd = std::exchange(other.fd, -1);
    }

    return *this;
}

std::span<const std::byte> TCPConnection::read(const std::span<std::byte> storage) const
{
    if (storage.empty())
        throw std::invalid_argument("TCP receive buffer is empty.");

    ssize_t received_count;

    do {
        received_count = recv(fd, storage.data(), storage.size(), 0);
    } while (received_count == -1 && errno == EINTR);

    if (received_count == -1)
        throw std::system_error(errno, std::generic_category(), "recv");

    return storage.first(static_cast<std::size_t>(received_count));
}

void TCPConnection::write_all(std::span<const std::byte> data) const
{
    while (!data.empty()) {
        const auto written_count = write(data);
        if (written_count == 0)
            throw std::system_error(std::make_error_code(std::errc::connection_reset), "send returned zero bytes");

        data = data.subspan(written_count);
    }
}

std::size_t TCPConnection::write(const std::span<const std::byte> data) const
{
    if (data.empty())
        return 0;

    ssize_t written_count;

    do {
        written_count = send(fd, data.data(), data.size(), MSG_NOSIGNAL);
    } while (written_count == -1 && errno == EINTR);

    if (written_count == -1)
        throw std::system_error(errno, std::generic_category(), "send");

    return written_count;
}

int TCPConnection::get_connection_fd(const std::string_view host, const std::uint16_t port)
{
    constexpr addrinfo hints{.ai_flags = 0,
                             .ai_family = AF_UNSPEC,
                             .ai_socktype = SOCK_STREAM,
                             .ai_protocol = IPPROTO_TCP,
                             .ai_addrlen = 0,
                             .ai_addr = nullptr,
                             .ai_canonname = nullptr,
                             .ai_next = nullptr};

    const std::string host_str{host};
    const auto port_str = std::to_string(port);

    /*
     * Given the specification (IPv4 or IPv6 on TCP) and the peer's host and port, retrieve a linked list of address
     * candidates. The candidate list is dynamically allocated so must be freed.
     */
    addrinfo* result_head = nullptr;
    if (const auto rc = getaddrinfo(host_str.c_str(), port_str.c_str(), &hints, &result_head); rc != 0)
        throw std::runtime_error(gai_strerror(rc));

    // Iterate through each of the returned candidates until we find one to which we can connect.
    for (const auto* candidate = result_head; candidate != nullptr; candidate = candidate->ai_next)
        if (const int fd = socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol); fd != -1) {
            if (connect(fd, candidate->ai_addr, candidate->ai_addrlen) == 0) {
                freeaddrinfo(result_head);
                return fd;
            }

            close(fd);
        }

    freeaddrinfo(result_head);
    throw std::system_error(errno, std::generic_category(), "connect");
}

int TCPConnection::get_accept_fd(const std::uint16_t port,
                                 const std::function<void(uint16_t)>& on_listening,
                                 const std::stop_token& stop_token)
{
    const auto listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
        throw std::system_error(errno, std::generic_category(), "socket");

    try {
        constexpr int enabled = 1;
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) == -1)
            throw std::system_error(errno, std::generic_category(), "setsockopt");

        sockaddr_in address{.sin_family = AF_INET,
                            .sin_port = htons(port),
                            .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)},
                            .sin_zero = {}};

        // Bind the listener to the address and listen.
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1)
            throw std::system_error(errno, std::generic_category(), "bind");

        if (listen(listener, 1) == -1)
            throw std::system_error(errno, std::generic_category(), "listen");

        if (on_listening) {
            // If a callback was provided, get the bound port (possibly ephemeral) and report it through the callback.
            socklen_t address_size = sizeof(address);
            if (getsockname(listener, reinterpret_cast<sockaddr*>(&address), &address_size) == -1)
                throw std::system_error(errno, std::generic_category(), "getsockname");

            on_listening(ntohs(address.sin_port));
        }

        // Once a connection can be accepted with the blocking accept, clean up and return the FD of the connection.
        while (true)
            if (const auto connection_fd = polling_accept(listener, stop_token); connection_fd != -1) {
                close(listener);
                return connection_fd;
            }
    } catch (...) {
        close(listener);
        throw;
    }

    std::unreachable();
}

int TCPConnection::polling_accept(const int listener, const std::stop_token& stop_token)
{
    const auto indicate_cancellation = [] {
        throw std::system_error(std::make_error_code(std::errc::operation_canceled), "accept cancelled");
    };

    // If a stop has already been requested, don't even start.
    if (stop_token.stop_requested())
        indicate_cancellation();

    pollfd descriptor{.fd = listener, .events = POLLIN, .revents = 0};

    static constexpr int poll_interval_ms = 50;
    int poll_result;

    // Poll at fixed intervals until a stop has been requested, or some low-level error occurred.
    do {
        poll_result = poll(&descriptor, 1, poll_interval_ms);
    } while (poll_result == -1 && errno == EINTR && !stop_token.stop_requested());

    if (stop_token.stop_requested())
        indicate_cancellation();

    if (poll_result == -1)
        throw std::system_error(errno, std::generic_category(), "poll");

    if (poll_result == 0)
        return -1;

    if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
        throw std::system_error(std::make_error_code(std::errc::io_error), "listening socket failed");

    if ((descriptor.revents & POLLIN) == 0)
        return -1;

    // Attempt to connect until a stop has been requested, connection is established, or an error occurs.
    int connection = -1;

    do {
        connection = accept(listener, nullptr, nullptr);
    } while (connection == -1 && errno == EINTR && !stop_token.stop_requested());

    if (connection == -1) {
        if (stop_token.stop_requested())
            indicate_cancellation();

        throw std::system_error(errno, std::generic_category(), "accept");
    }

    return connection;
}

} // namespace firestorm
