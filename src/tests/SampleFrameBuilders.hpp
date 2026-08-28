/**
 * @file
 *
 * SampleFrameBuilders specification
 *
 * @author Oliver Dixon
 * @date 2026-08-28
 */

#ifndef FIRESTORM_SAMPLEFRAMEBUILDERS_HPP
#define FIRESTORM_SAMPLEFRAMEBUILDERS_HPP

#include <netinet/in.h>

#include <array>

#include "../EthernetFrameView.hpp"

namespace firestorm::test
{

/**
 * Provides a set of utility functions to build Ethernet frames for testing harnesses.
 */
class SampleFrameBuilders
{
public:
    /**
     * MAC address prefix for inspected Ethernet frame headers.
     */
    using MACPrefix = std::array<std::byte, 3>;

private:
    static constexpr MACPrefix default_source_mac = {std::byte{0x10}, std::byte{0x20}, std::byte{0x30}};
    static constexpr MACPrefix default_destination_mac = {std::byte{0x40}, std::byte{0x50}, std::byte{0x60}};

public:
    /**
     * Fixed-size Ethernet frame payload data type.
     */
    using Frame = std::array<std::byte, 64>;

    /**
     * Constructs a valid-enough Ethernet/IPv4 frame for routing tests.
     *
     * The passthrough router only inspects the complete byte span, but the remaining header fields are populated to
     * keep the test data representative.
     *
     * @param fill_byte Value with which to fill the non-metadata sections of the payload.
     * @param transport_protocol POSIX encoding of the chosen transport protocol (TCP, UDP, etc).
     * @param source_mac Source MAC address prefix
     * @param destination_mac Destination MAC address prefix
     *
     * @return Constructed Ethernet frame.
     */
    static constexpr Frame make_frame_payload(const std::byte fill_byte,
                                              const uint8_t transport_protocol = IPPROTO_TCP,
                                              const std::optional<MACPrefix> source_mac = {},
                                              const std::optional<MACPrefix> destination_mac = {}) noexcept
    {
        Frame frame{};
        frame.fill(fill_byte);

        // Destination MAC
        const auto& dest_prefix = destination_mac.value_or(default_destination_mac);
        frame[0] = dest_prefix[0];
        frame[1] = dest_prefix[1];
        frame[2] = dest_prefix[2];
        frame[3] = std::byte{0x01};
        frame[4] = std::byte{0x02};
        frame[5] = std::byte{0x03};

        // Source MAC
        const auto& src_prefix = source_mac.value_or(default_source_mac);
        frame[6] = src_prefix[0];
        frame[7] = src_prefix[1];
        frame[8] = src_prefix[2];
        frame[9] = std::byte{0x04};
        frame[10] = std::byte{0x05};
        frame[11] = std::byte{0x06};

        // IPv4 EtherType
        frame[12] = std::byte{0x08};
        frame[13] = std::byte{0x00};

        frame[14] = std::byte{0x45};               // IPv4, 20-byte header
        frame[23] = std::byte{transport_protocol}; // Transport protocol

        // Include a NUL in the payload so exact binary forwarding is exercised
        frame[40] = std::byte{0x00};

        return frame;
    }

    /**
     * Builds an EthernetFrameView given a 64-byte payload.
     *
     * @param payload Encoded Ethernet frame data into which to build a view.
     *
     * @return An EthernetFrameView backed by @p payload storage.
     */
    static constexpr EthernetFrameView make_frame_view(const std::span<const std::byte, 64> payload) noexcept
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
};

} // namespace firestorm::test

#endif // FIRESTORM_SAMPLEFRAMEBUILDERS_HPP
