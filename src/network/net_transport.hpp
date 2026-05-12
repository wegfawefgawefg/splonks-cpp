#pragma once

#include "effects/effect_id.hpp"
#include "network/net_ids.hpp"
#include "network/net_fuzzer.hpp"
#include "network/net_limits.hpp"
#include "frame_data_id.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace splonks::network {

constexpr std::size_t kReplicatedEntityStateSignatureEffectCount = 12;

struct NetEndpoint {
    std::string address = "127.0.0.1";
    std::uint16_t port = 0;
};

struct UdpPacket {
    NetEndpoint endpoint;
    std::array<std::uint8_t, kNetPacketMaxBytes> bytes{};
    std::size_t size = 0;
};

struct NetFuzzedOutgoingPacket {
    UdpPacket packet;
    std::uint64_t due_time_ms = 0;
    std::uint64_t sequence = 0;
};

class UdpSocket {
public:
    UdpSocket() = default;
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;
    ~UdpSocket();

    bool Open(std::uint16_t bind_port, std::string* error_out);
    void Close();
    bool IsOpen() const;
    std::uint16_t BoundPort() const;

    bool Send(const NetEndpoint& endpoint, const std::uint8_t* bytes, std::size_t size, std::string* error_out);
    std::optional<UdpPacket> Receive(std::string* error_out);

private:
    int fd_ = -1;
    std::uint16_t bound_port_ = 0;
};

struct NetRemoteEndpoint {
    std::vector<PlayerId> player_ids;
    NetEndpoint endpoint;
    std::uint64_t last_heard_frame = 0;
    std::uint64_t highest_acked_coordinator_order = 0;
    std::uint64_t pending_resync_start_order = 0;
};

struct NetRemotePlayerTarget {
    PlayerId player_id = kInvalidPlayerId;
    float start_pos_x = 0.0F;
    float start_pos_y = 0.0F;
    float pos_x = 0.0F;
    float pos_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
    float acc_x = 0.0F;
    float acc_y = 0.0F;
    float size_x = 0.0F;
    float size_y = 0.0F;
    float rotation = 0.0F;
    std::uint32_t health = 0;
    std::uint32_t coyote_time = 0;
    std::uint32_t fall_timer = 0;
    std::uint32_t stun_timer = 0;
    std::uint32_t projectile_contact_timer = 0;
    NetEntityId thrown_by_id = kInvalidNetEntityId;
    std::uint32_t movement_flags = 0;
    std::uint16_t projectile_contact_damage_amount = 0;
    std::uint16_t jump_hold_gravity_frames_remaining = 0;
    std::uint16_t jump_delay_frame_count = 0;
    std::uint16_t climb_detach_cooldown = 0;
    std::uint16_t hang_count = 0;
    std::uint16_t holding_timer = 0;
    std::uint16_t bomb_throw_delay_countdown = 0;
    std::uint16_t rope_throw_delay_countdown = 0;
    std::uint16_t attack_delay_countdown = 0;
    std::uint16_t equip_delay_countdown = 0;
    std::uint16_t thrown_immunity_timer = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
    std::uint16_t animation_frame = 0;
    std::uint8_t facing = 0;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t has_physics = 1;
    std::uint8_t can_collide = 1;
    std::uint8_t can_apply_projectile_contact = 1;
    std::uint8_t projectile_contact_damage_type = 0;
    std::uint8_t hang_side = 0;
    std::uint8_t animate = 1;
    std::uint8_t animation_loop = 1;
    std::uint8_t animation_finished = 0;
    std::uint32_t sequence = 0;
    std::uint64_t interpolation_start_frame = 0;
    std::uint64_t last_received_frame = 0;
};

struct NetRemoteEntityRenderTarget {
    NetEntityId entity_id = kInvalidNetEntityId;
    float start_pos_x = 0.0F;
    float start_pos_y = 0.0F;
    float target_pos_x = 0.0F;
    float target_pos_y = 0.0F;
    std::uint64_t interpolation_start_frame = 0;
};

struct NetReplicatedEntityStateSignature {
    NetEntityId entity_id = kInvalidNetEntityId;
    NetEntityId entity_a_id = kInvalidNetEntityId;
    NetEntityId entity_b_id = kInvalidNetEntityId;
    NetEntityId entity_c_id = kInvalidNetEntityId;
    NetEntityId entity_d_id = kInvalidNetEntityId;
    NetEntityId holding_id = kInvalidNetEntityId;
    NetEntityId held_by_id = kInvalidNetEntityId;
    NetEntityId back_id = kInvalidNetEntityId;
    NetEntityId buyable_shop_owner_id = kInvalidNetEntityId;
    FrameDataId buyable_display_icon_animation_id = kInvalidFrameDataId;
    FrameDataId animation_id = kInvalidFrameDataId;
    float pos_x = 0.0F;
    float pos_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
    float acc_x = 0.0F;
    float acc_y = 0.0F;
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    float counter_c = 0.0F;
    float counter_d = 0.0F;
    float threshold_a = 0.0F;
    float threshold_b = 0.0F;
    float light_strength = 0.0F;
    float light_color_r = 1.0F;
    float light_color_g = 1.0F;
    float light_color_b = 1.0F;
    std::int32_t light_radius = 0;
    float rotation = 0.0F;
    float animation_speed = 1.0F;
    std::uint32_t health = 0;
    std::uint32_t coyote_time = 0;
    std::uint32_t fall_timer = 0;
    std::uint32_t stun_timer = 0;
    std::uint32_t projectile_contact_timer = 0;
    std::uint32_t buyable_display_quantity = 0;
    std::uint16_t animation_frame = 0;
    std::int32_t point_a_x = 0;
    std::int32_t point_a_y = 0;
    std::int32_t point_b_x = 0;
    std::int32_t point_b_y = 0;
    std::int32_t point_c_x = 0;
    std::int32_t point_c_y = 0;
    std::int32_t point_d_x = 0;
    std::int32_t point_d_y = 0;
    std::uint32_t movement_flags = 0;
    std::uint32_t runtime_flags = 0;
    std::uint8_t effect_count = 0;
    std::array<EffectId, kReplicatedEntityStateSignatureEffectCount> effect_ids{};
    std::array<std::int32_t, kReplicatedEntityStateSignatureEffectCount> effect_counts{};
    std::array<float, kReplicatedEntityStateSignatureEffectCount> effect_values{};
    std::array<std::uint32_t, kReplicatedEntityStateSignatureEffectCount> effect_frames{};
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t active = 0;
    std::uint8_t has_physics = 0;
    std::uint8_t can_collide = 0;
    std::uint8_t can_apply_projectile_contact = 0;
    std::uint8_t damage_vulnerability = 0;
    std::uint8_t facing = 0;
    std::uint8_t ai_state = 0;
    std::uint8_t wanted = 0;
    std::uint8_t holding = 0;
    std::uint8_t render_enabled = 1;
    std::uint8_t attachment_mode = 0;
    std::uint8_t draw_layer = 0;
    std::uint32_t money = 0;
    std::int32_t stage_exit_id = -1;
    std::uint8_t buyable_active = 0;
    std::uint8_t animate = 0;
    std::uint8_t animation_loop = 1;
    std::uint8_t animation_finished = 0;
};

struct NetReplicatedEntityStateCache {
    NetReplicatedEntityStateSignature signature;
};

struct NetReplicatedFluidCellSignature {
    std::int32_t tile_x = 0;
    std::int32_t tile_y = 0;
    std::uint16_t tile = 0;
    std::int32_t amount = 0;
    std::int32_t velocity_x = 0;
    std::int32_t velocity_y = 0;
    std::int32_t gravity_x = 0;
    std::int32_t gravity_y = 0;
    std::int32_t temp_gravity_x = 0;
    std::int32_t temp_gravity_y = 0;
    std::int32_t gravity_strength = 0;
};

struct NetReplicatedFluidCellCache {
    NetReplicatedFluidCellSignature signature;
    std::uint8_t empty_resend_frames_remaining = 0;
};

struct NetTransportRuntime {
    UdpSocket socket;
    std::vector<NetRemoteEndpoint> remotes;
    std::vector<NetRemotePlayerTarget> remote_player_targets;
    std::vector<NetRemoteEntityRenderTarget> remote_entity_render_targets;
    std::vector<NetReplicatedEntityStateCache> replicated_entity_state_cache;
    std::vector<NetReplicatedFluidCellCache> replicated_fluid_cell_cache;
    std::vector<PlayerId> preferred_player_ids;
    NetEndpoint coordinator_endpoint;
    bool pending_stage_sync = false;
    StageInstanceId pending_stage_instance_id = kInvalidStageInstanceId;
    std::uint32_t pending_stage_seed = 1;
    std::string pending_quest_id;
    std::string pending_quest_stage_id;
    bool join_request_pending = false;
    std::uint32_t join_request_retry_frames = 0;
    std::uint32_t next_snapshot_sequence = 1;
    std::uint32_t snapshot_send_interval_frames = 3;
    float remote_interpolation_strength = 0.35F;
    std::uint32_t remote_interpolation_delay_frames = 3;
    float remote_snap_distance = 32.0F;
    NetFuzzerConfig fuzzer_config;
    NetFuzzerStats fuzzer_stats;
    std::vector<NetFuzzedOutgoingPacket> fuzzed_outgoing_packets;
    std::uint64_t next_fuzzed_packet_sequence = 1;
    std::uint64_t fuzzer_next_bandwidth_send_time_ms = 0;
    std::uint32_t fuzzer_rng_state = 0xA341316CU;
    std::uint32_t fuzzer_burst_packets_remaining = 0;
    std::string last_error;
    bool capture_outgoing_packets = false;
    std::vector<UdpPacket> captured_packets;

    static NetTransportRuntime New();
};

bool EndpointsEqual(const NetEndpoint& a, const NetEndpoint& b);
std::string EndpointToString(const NetEndpoint& endpoint);

} // namespace splonks::network
