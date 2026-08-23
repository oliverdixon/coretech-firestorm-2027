/**
 * @file
 *
 * LazyFrameSink implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "LazyFrameSink.hpp"

#include "Logger.hpp"

namespace firestorm
{

LazyFrameSink::LazyFrameSink(const std::string_view host, const std::uint16_t port) :
    host(host),
    port(port),
    worker([this](const std::stop_token& stop_token) {
        run_worker(stop_token);
    })
{
}

void LazyFrameSink::send(const EthernetFrameView& frame)
{
    despatch_queue.produce(frame);
}

void LazyFrameSink::print_summary() const
{
    const auto [frames_sent, frames_dropped] = this->statistics.load();

    LOG_F_INFO("To the endpoint {}:{}, {} frames have been sent, and {} have been dropped.",
               host,
               port,
               frames_sent,
               frames_dropped);
}

void LazyFrameSink::reset_statistics() noexcept
{
    connection_metadata.store({});
    this->statistics.store({});
}

void LazyFrameSink::run_worker(const std::stop_token& stop_token)
{
    while (!stop_token.stop_requested())
        if (auto frame = despatch_queue.wait_consume(stop_token); frame.has_value()) {
            if (!transport.has_value())
                attempt_connection();

            auto new_statistics = statistics.load();

            if (transport.has_value()) {
                transport->write_all(frame->bytes);
                ++new_statistics.frames_sent;
            } else
                ++new_statistics.frames_dropped;

            statistics.store(new_statistics);
        }
}

void LazyFrameSink::attempt_connection()
{
    auto new_metadata = connection_metadata.load();

    if (new_metadata.connection_cooloff == 0)
        try {
            transport.emplace(host, port);
            LOG_F_INFO("Established forwarding TCP connection to {}:{}.", host, port);
        } catch (const std::exception& error) {
            if (new_metadata.failed_connection_attempts < sizeof(new_metadata.connection_cooloff) * 8)
                // Exponential back-off.
                new_metadata.connection_cooloff = 1U << new_metadata.failed_connection_attempts;
            ++new_metadata.failed_connection_attempts;

            LOG_F_WARNING("Could not establish connection to {}:{} because {}. Will try again in {} packet{}.",
                          host,
                          port,
                          error.what(),
                          new_metadata.connection_cooloff,
                          new_metadata.connection_cooloff == 1 ? "" : "s");
        }
    else
        --new_metadata.connection_cooloff;

    connection_metadata.store(new_metadata);
}

} // namespace firestorm
