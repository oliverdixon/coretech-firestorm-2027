/**
 * @file
 *
 * AsyncFrameSink implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "AsyncFrameSink.hpp"

#include "Logger.hpp"

namespace firestorm
{

AsyncFrameSink::AsyncFrameSink(const std::string_view host,
                               const std::uint16_t port,
                               const std::size_t maximum_buffered_bytes) :
    host(host),
    port(port),
    maximum_buffered_bytes(maximum_buffered_bytes),
    worker([this] {
        run_worker();
    })
{
    if (maximum_buffered_bytes == 0)
        throw std::invalid_argument("AsyncFrameSink cannot have an empty buffer.");
}

AsyncFrameSink::~AsyncFrameSink() noexcept
{
    try {
        finish();
    } catch (...) {
        // Swallow exceptions in destructor.
    }
}

void AsyncFrameSink::enqueue(std::shared_ptr<const std::vector<std::byte>> frame)
{
    if (!frame || frame->empty())
        throw std::invalid_argument("Cannot enqueue an empty frame");

    const auto frame_size = frame->size();
    if (frame_size > maximum_buffered_bytes)
        throw std::length_error("Frame exceeds the maximum buffer capacity of the sink.");

    std::unique_lock lock(mutex);
    buffer_available.wait(lock, [this, frame_size] {
        return worker_failure != nullptr || closing || buffered_bytes + frame_size <= maximum_buffered_bytes;
    });

    if (worker_failure)
        std::rethrow_exception(worker_failure);

    if (closing)
        throw std::logic_error("Cannot enqueue into a closing or closed sink.");

    frame_queue.push_back(std::move(frame));
    buffered_bytes += frame_size;
    lock.unlock();
    frame_available.notify_one();
}

void AsyncFrameSink::finish()
{
    {
        std::scoped_lock lock(mutex);
        closing = true;
    }

    frame_available.notify_all();
    buffer_available.notify_all();

    if (worker.joinable())
        worker.join();

    std::scoped_lock lock(mutex);
    if (worker_failure)
        std::rethrow_exception(worker_failure);
}

void AsyncFrameSink::print_summary() const
{
    LOG_F_INFO("To the endpoint {}:{}, {} frames have been sent, and {} have been dropped.",
               host,
               port,
               frames_sent.load(),
               frames_dropped.load());
}

void AsyncFrameSink::reset_statistics() noexcept
{
    failed_connection_attempts = 0;
    connection_cooloff = 0;
    frames_sent.store(0);
    frames_dropped.store(0);
}

void AsyncFrameSink::attempt_connection()
{
    if (connection_cooloff == 0)
        try {
            transport.emplace(host, port);
            LOG_F_INFO("Established forwarding TCP connection to {}:{}.", host, port);
        } catch (const std::exception& error) {
            if (failed_connection_attempts < sizeof(connection_cooloff) * 8)
                // Exponential back-off.
                    connection_cooloff = 1U << failed_connection_attempts;
            ++failed_connection_attempts;

            LOG_F_WARNING("Could not establish connection to {}:{} because {}. Will try again in {} packet{}.",
                          host,
                          port,
                          error.what(),
                          connection_cooloff,
                          connection_cooloff == 1 ? "" : "s");
        }
    else
        --connection_cooloff;
}

void AsyncFrameSink::run_worker() noexcept
{
    try {
        while (true) {
            std::shared_ptr<const std::vector<std::byte>> shared_frame;

            {
                std::unique_lock lock(mutex);
                frame_available.wait(lock, [this] {
                    return closing || !frame_queue.empty();
                });

                if (frame_queue.empty()) {
                    if (closing)
                        return;

                    continue;
                }

                shared_frame = std::move(frame_queue.front());
                frame_queue.pop_front();
            }

            if (!transport.has_value())
                attempt_connection();

            if (transport.has_value()) {
                transport->write_all(*shared_frame);
                frames_sent.fetch_add(1);
            } else
                frames_dropped.fetch_sub(1);

            {
                std::scoped_lock lock(mutex);
                buffered_bytes -= shared_frame->size();
            }

            buffer_available.notify_all();
        }
    } catch (...) {
        {
            std::scoped_lock lock(mutex);
            if (!worker_failure)
                worker_failure = std::current_exception();
            closing = true;
        }

        frame_available.notify_all();
        buffer_available.notify_all();
    }
}

} // namespace firestorm
