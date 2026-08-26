/**
 * @file
 *
 * PassthroughAsyncFrameRouter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "PassthroughAsyncFrameRouter.hpp"

#include "EthernetFrameView.hpp"
#include "Logger.hpp"

namespace firestorm
{

void PassthroughAsyncFrameRouter::route(const EthernetFrameView& view)
{
    ++frame_count;
    tcp_sink.enqueue(std::make_shared<const std::vector<std::byte>>(view.bytes.begin(), view.bytes.end()));
}

void PassthroughAsyncFrameRouter::drain_and_report()
{
    const auto [sent, dropped] = tcp_sink.drain();
    LOG_F_INFO("Under the passthrough policy, {} frames have been forwarded.", frame_count);
    LOG_F_INFO("To the endpoint {}, {} frames were sent, and {} were dropped.", tcp_sink.endpoint(), sent, dropped);

    frame_count = 0;
    malicious_count = 0;
}

} // namespace firestorm
