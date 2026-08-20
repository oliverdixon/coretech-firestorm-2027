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
        } catch (const std::exception& error) {
            std::println(std::cerr,
                         "Could not establish connection to {}:{}. Destined frames will be dropped.\nReason: {}\n",
                         host,
                         port,
                         error.what());
        }

    if (transport.has_value())
        transport->write_all(frame);
}

} // namespace firestorm
