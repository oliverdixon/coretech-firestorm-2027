/**
 * @file
 *
 * LoopbackTCPReceiver implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-28
 */

#include "LoopbackTCPReceiver.hpp"

#include "../TCPConnection.hpp"

namespace firestorm::test
{

LoopbackTCPReceiver::LoopbackTCPReceiver()
    : LoopbackTCPReceiver(0)
{
}

LoopbackTCPReceiver::LoopbackTCPReceiver(const std::uint16_t requested_port)
{
    auto ready = std::make_shared<std::promise<std::uint16_t>>();
    auto port_future = ready->get_future();

    worker = std::jthread([this, ready, requested_port](const std::stop_token& stop_token) {
        receive(stop_token, ready, requested_port);
    });

    assigned_port = port_future.get();  // Wait for the future to be satisfied.
}

LoopbackTCPReceiver::~LoopbackTCPReceiver() noexcept
{
    worker.request_stop();

    if (worker.joinable())
        worker.join();
}

void LoopbackTCPReceiver::wait()
{
    if (worker.joinable())
        worker.join();

    if (worker_failure)
        std::rethrow_exception(worker_failure);

    wait_completed = true;
}

std::uint16_t LoopbackTCPReceiver::get_assigned_port() const noexcept
{
    return assigned_port;
}

std::span<const std::byte> LoopbackTCPReceiver::observe_received_bytes() const
{
    if (!wait_completed)
        throw std::logic_error("The receiver is not ready to be observed.");

    return received_bytes;
}

void LoopbackTCPReceiver::receive(const std::stop_token& stop_token,
                                  std::shared_ptr<std::promise<std::uint16_t>> ready,
                                  const uint16_t requested_port)
{
    bool listening_announced = false;

    try {
        const TCPConnection connection{requested_port,
                                       [&listening_announced, &ready](const std::uint16_t port) {
                                           if (!listening_announced) {
                                               ready->set_value(port);
                                               listening_announced = true;
                                           }
                                       },
                                       stop_token};

        if (stop_token.stop_requested())
            return;

        std::array<std::byte, 4096> storage{};
        for (auto bytes = connection.read(storage); !bytes.empty(); bytes = connection.read(storage))
            received_bytes.insert(received_bytes.end(), bytes.cbegin(), bytes.cend());
    } catch (...) {
        const auto failure = std::current_exception();

        if (!listening_announced) {
            try {
                ready->set_exception(failure);
            } catch (...) {
                // Promise was already satisfied.
            }
        }

        if (!stop_token.stop_requested())
            worker_failure = failure;
    }
}

} // namespace firestorm::test
