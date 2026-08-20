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
 * @todo Document
 */
class LazyFrameSink
{
public:
    LazyFrameSink(std::string_view host, std::uint16_t port);

    void send(std::span<const std::byte> frame);

private:
    std::string host;
    std::uint16_t port;

    bool attempted = false;
    std::optional<TCPConnection> transport;
};

} // namespace firestorm

#endif // FIRESTORM_LAZYFRAMESINK_HPP
