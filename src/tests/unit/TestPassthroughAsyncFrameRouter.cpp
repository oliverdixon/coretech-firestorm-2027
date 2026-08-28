/**
 * @file
 *
 * TestPassthroughAsyncFrameRouter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-27
 */

#include "../../EthernetFrameView.hpp"
#include "../../PassthroughAsyncFrameRouter.hpp"
#include "../LoopbackTCPReceiver.hpp"
#include "../TestHarness.hpp"

using namespace firestorm;
using namespace firestorm::test;

namespace
{

/**
 * Constructs a valid-enough Ethernet/IPv4 frame for routing tests.
 *
 * The passthrough router only inspects the complete byte span, but the remaining header fields are populated to keep
 * the test data representative.
 *
 * @param marker Value used to distinguish the frame payload.
 *
 * @return Constructed Ethernet frame.
 */
constexpr std::array<std::byte, 64> make_test_frame(const std::byte marker) noexcept
{
    std::array<std::byte, 64> frame{};
    frame.fill(marker);

    // Destination MAC.
    frame[0] = std::byte{0x00};
    frame[1] = std::byte{0x1A};
    frame[2] = std::byte{0xA0};
    frame[3] = std::byte{0x01};
    frame[4] = std::byte{0x02};
    frame[5] = std::byte{0x03};

    // Source MAC.
    frame[6] = std::byte{0x10};
    frame[7] = std::byte{0x20};
    frame[8] = std::byte{0x30};
    frame[9] = std::byte{0x04};
    frame[10] = std::byte{0x05};
    frame[11] = std::byte{0x06};

    // IPv4 EtherType.
    frame[12] = std::byte{0x08};
    frame[13] = std::byte{0x00};

    // IPv4, 20-byte header.
    frame[14] = std::byte{0x45};

    // TCP protocol.
    frame[23] = std::byte{0x06};

    // Include a NUL in the payload so exact binary forwarding is exercised.
    frame[40] = std::byte{0x00};

    return frame;
}

EthernetFrameView make_frame_view(const std::span<const std::byte, 64> payload)
{
    return {
        .bytes = payload,
        .src_mac_prefix = payload.subspan<6, 3>(),
        .dst_mac_prefix = payload.first<3>(),
        .ip_header = payload.subspan<14, 20>(),
        .ip_payload = payload.subspan<34>(),
        .ip_protocol = std::to_integer<std::uint8_t>(payload[23])
    };
}

/**
 * @test Verifies that a single well-formed frame is passed through to the TCP receiver.
 *
 * @param harness Active harness.
 */
void routes_single_frame(TestHarness& harness)
{
    LoopbackTCPReceiver receiver;
    PassthroughAsyncFrameRouter router{"127.0.0.1", receiver.get_assigned_port()};
    constexpr auto frame = make_test_frame(std::byte{0xAA});

    router.route(make_frame_view(frame));
    router.drain_and_report();

    receiver.wait();
    CHECK(std::ranges::equal(receiver.observe_received_bytes(), frame));
}

/**
 * @test Verifies that frames are forwarded in submission order.
 *
 * @param harness Active harness.
 */
void routes_frames_in_order(TestHarness& harness)
{
    LoopbackTCPReceiver receiver;
    PassthroughAsyncFrameRouter router{"127.0.0.1", receiver.get_assigned_port()};

    constexpr auto first_frame = make_test_frame(std::byte{0x11});
    constexpr auto second_frame = make_test_frame(std::byte{0x22});
    constexpr auto third_frame = make_test_frame(std::byte{0x33});

    router.route(make_frame_view(first_frame));
    router.route(make_frame_view(second_frame));
    router.route(make_frame_view(third_frame));

    router.drain_and_report();

    receiver.wait();
    CHECK(std::ranges::equal(receiver.observe_received_bytes(),
                             TestHarness::concatenate(first_frame, second_frame, third_frame)));
}

/**
 * @test Verifies that the router immediately rejects empty frames.
 *
 * @param harness Active harness.
 */
void reject_empty_frames(TestHarness& harness)
{
    PassthroughAsyncFrameRouter router;

    static constexpr std::array<std::byte, 3> empty_prefix{};
    constexpr EthernetFrameView empty{
        .bytes = {},
        .src_mac_prefix = empty_prefix,
        .dst_mac_prefix = empty_prefix,
        .ip_header = {},
        .ip_payload = {},
        .ip_protocol = 0
    };

    CHECK_THROWS(router.route(empty), std::invalid_argument);
    router.drain_and_report();
}

/**
 * Verifies that draining an unused router completes successfully.
 *
 * @param harness Active harness.
 */
void drains_without_frames(TestHarness& harness)
{
    PassthroughAsyncFrameRouter router;
    router.drain_and_report();
    CHECK(true);
}

/**
 * @test Verifies that the router can be used following a full drain.
 *
 * Draining closes the first TCP connection and resets the frame sink. A second receiver is established on the same port
 * before the next frame is submitted.
 *
 * @param harness Active harness.
 */
void reuse_after_drain(TestHarness& harness)
{
    LoopbackTCPReceiver first_receiver;
    PassthroughAsyncFrameRouter router{"127.0.0.1", first_receiver.get_assigned_port()};

    constexpr auto first_frame = make_test_frame(std::byte{0x11});
    router.route(make_frame_view(first_frame));
    router.drain_and_report();
    first_receiver.wait();
    CHECK(std::ranges::equal(first_receiver.observe_received_bytes(), first_frame));

    LoopbackTCPReceiver second_receiver{first_receiver.get_assigned_port()};
    constexpr auto second_frame = make_test_frame(std::byte{0x22});
    router.route(make_frame_view(second_frame));
    router.drain_and_report();
    second_receiver.wait();
    CHECK(std::ranges::equal(second_receiver.observe_received_bytes(), second_frame));
}

} // namespace

/**
 * Executes selected PassthroughAsyncFrameRouter tests with a TestHarness.
 *
 * For the meanings of @p argc and @p argv, see TestHarness::run documentation.
 *
 * @param argc Argument count
 * @param argv Argument vector
 *
 * @return Zero if all executed tests passed; non-zero otherwise.
 */
int main(const int argc, const char* const* const argv)
{
    // clang-format off

    return TestHarness::run(argc, argv, {
        {
            .id = "routes_single_frame",
            .name = "Routes a single frame correctly",
            .function = routes_single_frame
        },
        {
            .id = "routes_frames_in_order",
            .name = "Routes frames in order of submission",
            .function = routes_frames_in_order
        },
        {
            .id = "reject_empty_frames",
            .name = "Routes rejects empty frames",
            .function = reject_empty_frames
        },
        {
            .id = "drains_without_frames",
            .name = "Draining an unused router completes successfully",
            .function = drains_without_frames
        },
        {
            .id = "reuse_after_drain",
            .name = "Router can be reused following a full drain",
            .function = reuse_after_drain
        },
    });

    // clang-format on
}
