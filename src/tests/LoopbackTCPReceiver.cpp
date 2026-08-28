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
{
    auto ready = std::make_shared<std::promise<std::uint16_t>>();
    auto port_future = ready->get_future();

    worker = std::jthread([this, ready](const std::stop_token& stop_token) {
        receive(stop_token, ready);
    });

    assigned_port = port_future.get(); // Wait for the future to be satisfied.
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

void LoopbackTCPReceiver::receive(const std::stop_token& stop_token, std::shared_ptr<std::promise<std::uint16_t>> ready)
{
    bool listening_announced = false;

    try {
        const TCPConnection connection{0, [&listening_announced, &ready](const std::uint16_t port) {
                                     if (!listening_announced) {
                                         ready->set_value(port);
                                         listening_announced = true;
                                     }
                                 }};

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

        worker_failure = failure;
    }
}

} // namespace firestorm::test
