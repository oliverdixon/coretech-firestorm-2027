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
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace firestorm
{

TCPConnection::TCPConnection(
        const std::string_view host,
        const std::uint16_t port
) :
    fd(get_connection_fd(
            host,
            port
    ))
{
}

TCPConnection::TCPConnection(
        const std::uint16_t port
) :
    fd(get_accept_fd(port))
{
}

TCPConnection::~TCPConnection() noexcept
{
    if (fd != -1)
        close(fd);
}

TCPConnection::TCPConnection(
        TCPConnection&& other
) noexcept :
    fd(std::exchange(
            other.fd,
            -1
    ))
{
}

TCPConnection& TCPConnection::operator=(
        TCPConnection&& other
) noexcept
{
    if (this != &other) {
        if (fd != -1)
            close(fd);

        fd = std::exchange(other.fd, -1);
    }

    return *this;
}

int TCPConnection::get_connection_fd(
        const std::string_view host,
        const std::uint16_t port
)
{
    constexpr addrinfo hints{.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM};

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

int TCPConnection::get_accept_fd(
        const std::uint16_t port
)
{
    const auto listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1)
        throw std::system_error(errno, std::generic_category(), "socket");

    try {
        constexpr int enabled = 1;
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) == -1)
            throw std::system_error(errno, std::generic_category(), "setsockopt");

        sockaddr_in address{.sin_family = AF_INET, .sin_port = htons(port), .sin_addr = {.s_addr = htonl(INADDR_ANY)}};

        // Bind the listener to the address and listen.
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1)
            throw std::system_error(errno, std::generic_category(), "bind");

        if (listen(listener, 1) == -1)
            throw std::system_error(errno, std::generic_category(), "listen");

        // Once a connection can be accepted with the blocking accept, clean up and return the FD of the connection.
        int fd;
        do {
            fd = accept(listener, nullptr, nullptr);
        } while (fd == -1 && errno == EINTR);

        if (fd == -1)
            throw std::system_error(errno, std::generic_category(), "accept");

        close(listener);
        return fd;

    } catch (...) {
        close(listener);
        throw;
    }
}

} // namespace firestorm
