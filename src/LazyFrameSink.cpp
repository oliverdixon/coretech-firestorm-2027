/**
 * @file
 *
 * LazyFrameSink implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "LazyFrameSink.hpp"

#include <cmath>
#include <iostream>

#include "Logger.hpp"

namespace firestorm
{

LazyFrameSink::LazyFrameSink(const std::string_view host, const std::uint16_t port) :
    host(host),
    port(port)
{
}

void LazyFrameSink::send(const std::span<const std::byte> frame)
{
    if (connection_cooloff == 0 && !transport.has_value())
        try {
            transport.emplace(host, port);
            LOG_F_INFO("Established forwarding TCP connection to {}:{}.", host, port);
        } catch (const std::exception& error) {
            connection_cooloff = 1U << failed_connection_attempts; // Exponential back-off.
            ++failed_connection_attempts;
            LOG_F_WARNING("Could not establish connection to {}:{} because {}. Will try again in {} packet{}.",
                          host,
                          port,
                          error.what(),
                          connection_cooloff,
                          connection_cooloff == 1 ? "" : "s");
        }

    if (transport.has_value()) {
        ++frames_sent;
        transport->write_all(frame);
    } else
        ++frames_dropped;
}

void LazyFrameSink::print_summary() const
{
    LOG_F_INFO("To the endpoint {}:{}, {} frames have been sent, and {} have been dropped.",
               host,
               port,
               frames_sent,
               frames_dropped);
}

void LazyFrameSink::reset_statistics() noexcept
{
    failed_connection_attempts = 0;
    connection_cooloff = 0;
    frames_sent = 0;
    frames_dropped = 0;
}

} // namespace firestorm
