#pragma once

#include <cstdint>

namespace splonks::network {

struct NetFuzzerConfig {
    bool enabled = false;
    float latency_ms = 0.0F;
    float jitter_ms = 0.0F;
    float packet_loss_percent = 0.0F;
    float duplicate_percent = 0.0F;
    std::uint32_t reorder_window_packets = 0;
    std::uint32_t bandwidth_cap_bytes_per_second = 0;
    bool burst_loss_enabled = false;
    float burst_loss_percent = 0.0F;
    float clock_drift_percent = 0.0F;

    static NetFuzzerConfig LanPreset();
    static NetFuzzerConfig SameHousePreset();
    static NetFuzzerConfig SameCityPreset();
    static NetFuzzerConfig SameStatePreset();
    static NetFuzzerConfig SameRegionPreset();
    static NetFuzzerConfig TexasToCaliforniaPreset();
    static NetFuzzerConfig CaliforniaToFloridaPreset();
    static NetFuzzerConfig UsCrossCountryPreset();
    static NetFuzzerConfig JapanToTexasPreset();
    static NetFuzzerConfig BadWifiPreset();
};

struct NetFuzzerStats {
    std::uint64_t packets_sent = 0;
    std::uint64_t packets_received = 0;
    std::uint64_t packets_dropped = 0;
    std::uint64_t packets_duplicated = 0;
    std::uint64_t packets_reordered = 0;
};

} // namespace splonks::network
