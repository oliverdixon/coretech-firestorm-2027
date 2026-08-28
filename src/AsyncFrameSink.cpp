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
                               const std::size_t maximum_buffered_bytes)
    : host(host),
      port(port),
      maximum_buffered_bytes(maximum_buffered_bytes)
{
    if (maximum_buffered_bytes == 0)
        throw std::invalid_argument("AsyncFrameSink cannot have an empty buffer.");

    worker = std::jthread([this] {
        run_worker();
    });
}

AsyncFrameSink::~AsyncFrameSink() noexcept
{
    try {
        drain_for_destruction();
    } catch (...) {
        // Swallow exceptions in destructor.
    }
}

void AsyncFrameSink::enqueue(std::shared_ptr<const std::vector<std::byte>> frame)
{
    if (!frame || frame->empty())
        throw std::invalid_argument("Cannot enqueue an empty frame.");

    const auto frame_size = frame->size();
    if (frame_size > maximum_buffered_bytes)
        throw std::length_error("Frame exceeds the maximum buffer capacity of the sink.");

    std::unique_lock lock(shared_state_mutex);

    /*
     * Wait until the buffer is available.
     *
     * If the connection gets closed, or the buffer exhausted, or the worker throws an exception, we propagate the
     * failure here instead of accepting frame.
     */
    buffer_available.wait(lock, [this, frame_size] {
        return shared_state.worker_failure != nullptr || shared_state.closing ||
               shared_state.buffered_bytes + frame_size <= maximum_buffered_bytes;
    });

    if (shared_state.worker_failure)
        std::rethrow_exception(shared_state.worker_failure);

    if (shared_state.closing)
        throw std::logic_error("Cannot enqueue into a closing or closed sink.");

    // If everything is OK, accept the frame by transferring ownership into the queue.
    shared_state.frames.push_back(std::move(frame));
    shared_state.buffered_bytes += frame_size;
    lock.unlock();

    // Indicate that a new frame is available to the worker thread.
    frame_available.notify_one();
}

std::pair<size_t, size_t> AsyncFrameSink::drain()
{
    // Do the destructive drain. This breaks RAII, so to preserve it we need to reset the class state explicitly.
    drain_for_destruction();

    // Preserve stats we care about and reset the class state.
    std::pair preserved_stats{statistics.frames_sent, statistics.frames_dropped};

    statistics.frames_sent = 0;
    statistics.frames_dropped = 0;
    statistics.connection_cooloff = 0;
    statistics.failed_connection_attempts = 0;

    {
        // Indicate that we've reset and can accept frames again.
        std::scoped_lock lock(shared_state_mutex);
        shared_state.buffered_bytes = 0;
        shared_state.frames.clear();
        shared_state.closing = false;
    }

    // Reset the transport and restart the worker. The transport will be lazily re-established by the worker.
    transport.reset();
    worker = std::jthread([this] {
        run_worker();
    });

    return preserved_stats;
}

std::string AsyncFrameSink::endpoint() const
{
    return std::format("{}:{}", host, port);
}

void AsyncFrameSink::attempt_connection()
{
    if (statistics.connection_cooloff == 0)
        try {
            transport.emplace(host, port);
            LOG_F_INFO("Established forwarding TCP connection to {}:{}.", host, port);
        } catch (const std::exception& error) {
            // Apply exponential back-off cooling off period.

            using CooloffT = decltype(statistics.connection_cooloff);
            constexpr auto digits = std::numeric_limits<CooloffT>::digits;
            if (statistics.failed_connection_attempts < digits)
                statistics.connection_cooloff = CooloffT{1} << statistics.failed_connection_attempts;

            ++statistics.failed_connection_attempts;

            LOG_F_WARNING("Could not establish connection to {}:{} because {}. Will try again in {} packet{}.",
                          host,
                          port,
                          error.what(),
                          statistics.connection_cooloff,
                          statistics.connection_cooloff == 1 ? "" : "s");
        }
    else
        --statistics.connection_cooloff;
}

void AsyncFrameSink::drain_for_destruction()
{
    {
        // Indicate that we're closing the connection so no more frames are accepted.
        std::scoped_lock lock(shared_state_mutex);
        shared_state.closing = true;
    }

    // Indicate to the workers to clean up the frames; wait for them; and rejoin.
    frame_available.notify_all();
    buffer_available.notify_all();

    if (worker.joinable())
        worker.join();

    {
        // Propagate any exceptions from the workers that drained the queue.
        std::scoped_lock lock(shared_state_mutex);
        if (shared_state.worker_failure)
            std::rethrow_exception(shared_state.worker_failure);
    }
}

void AsyncFrameSink::run_worker() noexcept
{
    std::size_t last_frame_size = 0;

    try {
        while (true) {
            std::shared_ptr<const std::vector<std::byte>> shared_frame;

            {
                std::unique_lock lock(shared_state_mutex);
                frame_available.wait(lock, [this] {
                    return shared_state.closing || !shared_state.frames.empty();
                });

                if (shared_state.frames.empty()) {
                    /*
                     * If we bailed out early, we either got an empty frame (so we skip it), or the connection is going
                     * down.
                     */
                    if (shared_state.closing)
                        return;

                    continue;
                }

                shared_frame = std::move(shared_state.frames.front());
                shared_state.frames.pop_front();
            }

            last_frame_size = shared_frame->size();

            // We own the frame in this scope. Send it to the endpoint; if it's not connected, retry now.
            if (!transport.has_value())
                attempt_connection();

            if (transport.has_value()) {
                transport->write_all(*shared_frame);
                ++statistics.frames_sent;
            } else
                ++statistics.frames_dropped;

            // TCPConnection::write_all didn't throw, so we can clean up here.
            {
                std::scoped_lock lock(shared_state_mutex);
                shared_state.buffered_bytes -= shared_frame->size();
            }

            last_frame_size = 0;
            buffer_available.notify_all();
        }
    } catch (...) {
        // Likely TCPConnection::write_all threw. Clean up the shared state to indicate that the frame was dropped.

        {
            std::scoped_lock lock(shared_state_mutex);
            shared_state.buffered_bytes -= last_frame_size;
            if (!shared_state.worker_failure)
                shared_state.worker_failure = std::current_exception();
            shared_state.closing = true;
        }

        last_frame_size = 0;
        ++statistics.frames_dropped;

        frame_available.notify_all();
        buffer_available.notify_all();
    }
}

} // namespace firestorm
