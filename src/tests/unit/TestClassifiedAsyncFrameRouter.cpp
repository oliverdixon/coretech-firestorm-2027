/**
 * @file
 *
 * TestClassifiedAsyncFrameRouter implementation
 *
 * @author Oliver Dixon
 * @date 2026-08-28
 */

#include "../../ClassifiedAsyncFrameRouter.hpp"
#include "../LoopbackTCPReceiver.hpp"
#include "../SampleFrameBuilders.hpp"
#include "../TestHarness.hpp"

using namespace firestorm;
using namespace firestorm::test;

namespace
{

/**
 * All MAC destination prefixes for which monitoring is indicated.
 */
constexpr std::array monitored_destinations{
    SampleFrameBuilders::MACPrefix{std::byte{0x00}, std::byte{0x1A}, std::byte{0xA0}},
    SampleFrameBuilders::MACPrefix{std::byte{0x00}, std::byte{0x1E}, std::byte{0xC9}},
    SampleFrameBuilders::MACPrefix{std::byte{0x00}, std::byte{0x15}, std::byte{0x5D}},
};

/**
 * All MAC source prefixes for which malicious handling is indicated.
 */
constexpr std::array malicious_sources{
    SampleFrameBuilders::MACPrefix{std::byte{0x00}, std::byte{0x0C}, std::byte{0x29}},
    SampleFrameBuilders::MACPrefix{std::byte{0x00}, std::byte{0x50}, std::byte{0x56}},
    SampleFrameBuilders::MACPrefix{std::byte{0x00}, std::byte{0x1C}, std::byte{0x14}},
};

/**
 * Routes frames and verifies the exact byte stream delivered to each endpoint.
 *
 * @param harness Active harness.
 * @param submitted All frames to submit to the router.
 * @param expected_tcp Expected data forwarded to the TCP sink.
 * @param expected_monitor Expected data forwarded to the monitoring sink.
 * @param expected_udp Expected data forwarded to the UDP sink.
 */
void verify_routing(TestHarness& harness,
                    const std::span<const SampleFrameBuilders::Frame> submitted,
                    const std::span<const std::byte> expected_tcp,
                    const std::span<const std::byte> expected_monitor,
                    const std::span<const std::byte> expected_udp)
{
    LoopbackTCPReceiver tcp_receiver;
    LoopbackTCPReceiver monitor_receiver;
    LoopbackTCPReceiver udp_receiver;

    ClassifiedAsyncFrameRouter router{"127.0.0.1",
                                      tcp_receiver.get_assigned_port(),
                                      monitor_receiver.get_assigned_port(),
                                      udp_receiver.get_assigned_port()};

    for (const auto& frame_payload : submitted)
        router.route(SampleFrameBuilders::make_frame_view(frame_payload));

    router.drain_and_report();

    tcp_receiver.wait();
    monitor_receiver.wait();
    udp_receiver.wait();

    CHECK(std::ranges::equal(tcp_receiver.observe_received_bytes(), expected_tcp));
    CHECK(std::ranges::equal(monitor_receiver.observe_received_bytes(), expected_monitor));
    CHECK(std::ranges::equal(udp_receiver.observe_received_bytes(), expected_udp));
}

/**
 * @test Verifies that packets matching any rule are forwarded to the suitable endpoint.
 *
 * @param harness Active harness.
 */
void routes_complete_policy_matrix(TestHarness& harness)
{
    // Only protocols
    constexpr auto just_tcp = SampleFrameBuilders::make_frame_payload(std::byte{0x01}, IPPROTO_TCP);
    constexpr auto just_udp = SampleFrameBuilders::make_frame_payload(std::byte{0x02}, IPPROTO_UDP);
    constexpr auto just_other = SampleFrameBuilders::make_frame_payload(std::byte{0x03}, IPPROTO_ICMP);

    // Malicious
    constexpr auto malicious_tcp =
        SampleFrameBuilders::make_frame_payload(std::byte{0x04}, IPPROTO_TCP, malicious_sources[0]);
    constexpr auto malicious_udp =
        SampleFrameBuilders::make_frame_payload(std::byte{0x05}, IPPROTO_UDP, malicious_sources[1]);
    constexpr auto malicious_other =
        SampleFrameBuilders::make_frame_payload(std::byte{0x06}, IPPROTO_ICMP, malicious_sources[2]);

    // Monitored
    constexpr auto monitored_tcp =
        SampleFrameBuilders::make_frame_payload(std::byte{0x04}, IPPROTO_TCP, {}, monitored_destinations[0]);
    constexpr auto monitored_udp =
        SampleFrameBuilders::make_frame_payload(std::byte{0x05}, IPPROTO_UDP, {}, monitored_destinations[1]);
    constexpr auto monitored_other =
        SampleFrameBuilders::make_frame_payload(std::byte{0x06}, IPPROTO_ICMP, {}, monitored_destinations[2]);

    // Malicious and monitored
    constexpr auto both_tcp = SampleFrameBuilders::make_frame_payload(
        std::byte{0x04}, IPPROTO_TCP, malicious_sources[0], monitored_destinations[0]);
    constexpr auto both_udp = SampleFrameBuilders::make_frame_payload(
        std::byte{0x05}, IPPROTO_UDP, malicious_sources[1], monitored_destinations[1]);
    constexpr auto both_other = SampleFrameBuilders::make_frame_payload(
        std::byte{0x06}, IPPROTO_ICMP, malicious_sources[2], monitored_destinations[2]);

    // All payloads to submit to the router
    constexpr std::array submitted{
        just_tcp,
        just_udp,
        just_other,

        malicious_tcp,
        malicious_udp,
        malicious_other,

        monitored_tcp,
        monitored_udp,
        monitored_other,

        both_tcp,
        both_udp,
        both_other,
    };

    const auto expected_tcp = TestHarness::concatenate(just_tcp, monitored_tcp);
    const auto expected_monitored =
        TestHarness::concatenate(monitored_tcp, monitored_udp, monitored_other, both_tcp, both_udp, both_other);
    const auto expected_udp = TestHarness::concatenate(just_udp, monitored_udp);

    verify_routing(harness, submitted, expected_tcp, expected_monitored, expected_udp);
}

} // namespace

/**
 * Executes selected ClassifiedAsyncFrameRouter tests with a TestHarness.
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
            .id = "routes_complete_policy_matrix",
            .name = "Routes any combination according to the policy",
            .function = routes_complete_policy_matrix
        },
    });

    // clang-format on
}
