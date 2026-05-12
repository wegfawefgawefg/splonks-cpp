#include "network/net_fuzzer.hpp"

namespace splonks::network {

NetFuzzerConfig NetFuzzerConfig::LanPreset() {
    NetFuzzerConfig config;
    config.enabled = true;
    config.latency_ms = 5.0F;
    config.jitter_ms = 1.0F;
    return config;
}

NetFuzzerConfig NetFuzzerConfig::SameHousePreset() {
    NetFuzzerConfig config;
    config.enabled = true;
    config.latency_ms = 2.0F;
    config.jitter_ms = 1.0F;
    return config;
}

NetFuzzerConfig NetFuzzerConfig::SameCityPreset() {
    NetFuzzerConfig config;
    config.enabled = true;
    config.latency_ms = 15.0F;
    config.jitter_ms = 4.0F;
    config.packet_loss_percent = 0.05F;
    return config;
}

NetFuzzerConfig NetFuzzerConfig::SameStatePreset() {
    NetFuzzerConfig config;
    config.enabled = true;
    config.latency_ms = 30.0F;
    config.jitter_ms = 8.0F;
    config.packet_loss_percent = 0.1F;
    return config;
}

NetFuzzerConfig NetFuzzerConfig::SameRegionPreset() {
    NetFuzzerConfig config;
    config.enabled = true;
    config.latency_ms = 35.0F;
    config.jitter_ms = 8.0F;
    config.packet_loss_percent = 0.2F;
    return config;
}

NetFuzzerConfig NetFuzzerConfig::TexasToCaliforniaPreset() {
    NetFuzzerConfig config;
    config.enabled = true;
    config.latency_ms = 55.0F;
    config.jitter_ms = 12.0F;
    config.packet_loss_percent = 0.3F;
    return config;
}

NetFuzzerConfig NetFuzzerConfig::CaliforniaToFloridaPreset() {
    NetFuzzerConfig config;
    config.enabled = true;
    config.latency_ms = 75.0F;
    config.jitter_ms = 15.0F;
    config.packet_loss_percent = 0.5F;
    return config;
}

NetFuzzerConfig NetFuzzerConfig::UsCrossCountryPreset() {
    NetFuzzerConfig config;
    config.enabled = true;
    config.latency_ms = 80.0F;
    config.jitter_ms = 15.0F;
    config.packet_loss_percent = 0.5F;
    return config;
}

NetFuzzerConfig NetFuzzerConfig::JapanToTexasPreset() {
    NetFuzzerConfig config;
    config.enabled = true;
    config.latency_ms = 150.0F;
    config.jitter_ms = 25.0F;
    config.packet_loss_percent = 1.0F;
    return config;
}

NetFuzzerConfig NetFuzzerConfig::BadWifiPreset() {
    NetFuzzerConfig config;
    config.enabled = true;
    config.latency_ms = 90.0F;
    config.jitter_ms = 60.0F;
    config.packet_loss_percent = 3.0F;
    config.burst_loss_enabled = true;
    config.burst_loss_percent = 8.0F;
    return config;
}

} // namespace splonks::network
