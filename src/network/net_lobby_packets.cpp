#include "network/net_lobby_internal.hpp"

#include "math_types.hpp"

#include <algorithm>
#include <chrono>
#include <string>

namespace splonks::network {

namespace {

std::uint64_t NowMilliseconds() {
    using Clock = std::chrono::steady_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()
        ).count()
    );
}

std::uint32_t NextFuzzerRandom(NetTransportRuntime& transport) {
    transport.fuzzer_rng_state = (transport.fuzzer_rng_state * 1664525U) + 1013904223U;
    return transport.fuzzer_rng_state;
}

float RandomUnit(NetTransportRuntime& transport) {
    constexpr float kScale = 1.0F / 16777216.0F;
    return static_cast<float>(NextFuzzerRandom(transport) >> 8U) * kScale;
}

bool PercentChance(NetTransportRuntime& transport, float percent) {
    if (percent <= 0.0F) {
        return false;
    }
    if (percent >= 100.0F) {
        return true;
    }
    return RandomUnit(transport) * 100.0F < percent;
}

float RandomSignedRange(NetTransportRuntime& transport, float magnitude) {
    if (magnitude <= 0.0F) {
        return 0.0F;
    }
    return ((RandomUnit(transport) * 2.0F) - 1.0F) * magnitude;
}

bool SendRawPacket(NetTransportRuntime& transport, const UdpPacket& packet) {
    UdpPacket packet_to_send = packet;
    UdpPacket relay_wrapped;
    if (WrapRealnetRelayPacket(transport, packet, relay_wrapped)) {
        if (relay_wrapped.size == 0)
            return true;
        packet_to_send = relay_wrapped;
    }
    std::string error;
    if (!transport.socket.Send(packet_to_send.endpoint,
                               packet_to_send.bytes.data(),
                               packet_to_send.size,
                               &error)) {
        transport.last_error = error;
        return false;
    }
    return true;
}

float CurrentLossPercent(NetTransportRuntime& transport) {
    const NetFuzzerConfig& config = transport.fuzzer_config;
    if (!config.burst_loss_enabled || config.burst_loss_percent <= 0.0F) {
        return config.packet_loss_percent;
    }

    if (transport.fuzzer_burst_packets_remaining == 0 && PercentChance(transport, 1.0F)) {
        transport.fuzzer_burst_packets_remaining = 8U + (NextFuzzerRandom(transport) % 25U);
    }
    if (transport.fuzzer_burst_packets_remaining == 0) {
        return config.packet_loss_percent;
    }

    transport.fuzzer_burst_packets_remaining -= 1U;
    return std::max(config.packet_loss_percent, config.burst_loss_percent);
}

std::uint64_t FuzzedDueTimeMilliseconds(
    NetTransportRuntime& transport,
    std::size_t packet_size,
    bool duplicate
) {
    const NetFuzzerConfig& config = transport.fuzzer_config;
    const std::uint64_t now_ms = NowMilliseconds();
    float delay_ms = config.latency_ms + RandomSignedRange(transport, config.jitter_ms);
    delay_ms = std::max(0.0F, delay_ms);

    if (config.reorder_window_packets > 0U) {
        const std::uint32_t max_reorder_ms = config.reorder_window_packets * 2U;
        delay_ms += static_cast<float>(NextFuzzerRandom(transport) % (max_reorder_ms + 1U));
        transport.fuzzer_stats.packets_reordered += 1U;
    }

    if (duplicate) {
        delay_ms += static_cast<float>(NextFuzzerRandom(transport) % 4U);
    }

    std::uint64_t due_ms = now_ms + static_cast<std::uint64_t>(RoundToInt(delay_ms));
    if (config.bandwidth_cap_bytes_per_second > 0U) {
        due_ms = std::max(due_ms, transport.fuzzer_next_bandwidth_send_time_ms);
        const float packet_ms =
            (static_cast<float>(packet_size) * 1000.0F) /
            static_cast<float>(config.bandwidth_cap_bytes_per_second);
        transport.fuzzer_next_bandwidth_send_time_ms =
            due_ms + std::max<std::uint64_t>(1U, static_cast<std::uint64_t>(CeilToInt(packet_ms)));
    }
    return due_ms;
}

void QueueFuzzedPacket(
    NetTransportRuntime& transport,
    const UdpPacket& packet,
    bool duplicate
) {
    NetFuzzedOutgoingPacket delayed;
    delayed.packet = packet;
    delayed.due_time_ms = FuzzedDueTimeMilliseconds(transport, packet.size, duplicate);
    delayed.sequence = transport.next_fuzzed_packet_sequence++;
    transport.fuzzed_outgoing_packets.push_back(delayed);
}

} // namespace

void SendEncodedPacket(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const EncodedNetPacket& encoded
) {
    if (transport.capture_outgoing_packets) {
        UdpPacket packet;
        packet.endpoint = endpoint;
        packet.size = std::min(encoded.size, packet.bytes.size());
        std::copy_n(encoded.bytes.begin(), packet.size, packet.bytes.begin());
        transport.captured_packets.push_back(packet);
        return;
    }

    UdpPacket packet;
    packet.endpoint = endpoint;
    packet.size = std::min(encoded.size, packet.bytes.size());
    std::copy_n(encoded.bytes.begin(), packet.size, packet.bytes.begin());

    transport.fuzzer_stats.packets_sent += 1U;
    if (!transport.fuzzer_config.enabled) {
        (void)SendRawPacket(transport, packet);
        return;
    }

    if (PercentChance(transport, CurrentLossPercent(transport))) {
        transport.fuzzer_stats.packets_dropped += 1U;
        return;
    }

    QueueFuzzedPacket(transport, packet, false);
    if (PercentChance(transport, transport.fuzzer_config.duplicate_percent)) {
        transport.fuzzer_stats.packets_duplicated += 1U;
        QueueFuzzedPacket(transport, packet, true);
    }
}

void FlushFuzzedOutgoingPackets(NetTransportRuntime& transport) {
    if (transport.fuzzed_outgoing_packets.empty()) {
        return;
    }

    const bool force_flush = !transport.fuzzer_config.enabled;
    const std::uint64_t now_ms = NowMilliseconds();
    std::sort(
        transport.fuzzed_outgoing_packets.begin(),
        transport.fuzzed_outgoing_packets.end(),
        [](const NetFuzzedOutgoingPacket& a, const NetFuzzedOutgoingPacket& b) {
            if (a.due_time_ms != b.due_time_ms) {
                return a.due_time_ms < b.due_time_ms;
            }
            return a.sequence < b.sequence;
        }
    );

    std::vector<NetFuzzedOutgoingPacket> pending;
    pending.reserve(transport.fuzzed_outgoing_packets.size());
    for (const NetFuzzedOutgoingPacket& delayed : transport.fuzzed_outgoing_packets) {
        if (force_flush || delayed.due_time_ms <= now_ms) {
            (void)SendRawPacket(transport, delayed.packet);
        } else {
            pending.push_back(delayed);
        }
    }
    transport.fuzzed_outgoing_packets = std::move(pending);
}

} // namespace splonks::network
