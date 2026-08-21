/**
 * @file
 *
 * LazyFrameSink implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "LazyFrameSink.hpp"

#include <iostream>
#include <print>

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
    if (!attempted && !transport.has_value())
        try {
            attempted = true;
            transport.emplace(host, port);
            LOG_F_INFO("Established forwarding TCP connection to {}:{}.", host, port);
        } catch (const std::exception& error) {
            LOG_F_WARNING("Could not establish connection to {}:{}. Destined frames will be dropped. Reason: {}",
                          host,
                          port,
                          error.what());
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
    frames_sent = 0;
    frames_dropped = 0;
}

} // namespace firestorm
