/**
 * @file
 *
 * PassthroughAsyncFrameRouter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "PassthroughAsyncFrameRouter.hpp"

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "EthernetFrameView.hpp"
#include "Logger.hpp"

namespace firestorm
{

PassthroughAsyncFrameRouter::PassthroughAsyncFrameRouter(const std::string_view host, const std::uint16_t port)
    : tcp_sink(host, port)
{
}

void PassthroughAsyncFrameRouter::route(const EthernetFrameView& view)
{
    tcp_sink.enqueue(std::make_shared<const std::vector<std::byte>>(view.bytes.begin(), view.bytes.end()));
    ++frame_count;
}

void PassthroughAsyncFrameRouter::drain_and_report()
{
    [[maybe_unused]] const auto [sent, dropped] = tcp_sink.drain();
    LOG_F_INFO("Under the passthrough policy, {} frames have been forwarded.", frame_count);
    LOG_F_INFO("To the endpoint {}, {} frames were sent, and {} were dropped.", tcp_sink.endpoint(), sent, dropped);

    frame_count = 0;
}

} // namespace firestorm
