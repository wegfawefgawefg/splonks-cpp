#include "cli_network_smoke.hpp"

#include "cli_network_smoke_internal.hpp"
#include "quest_stage_loader.hpp"

#include <exception>
#include <iostream>

namespace splonks {

namespace {

bool CheckCompactActionRequestPacketSmoke() {
    network::ActionRequestMessagesPacket packet;
    packet.messages.push_back(network::ActionRequestMessageEntry{
        .message_id = 1,
        .source_player_id = 2,
        .stage_instance_id = 3,
        .source_local_frame = 4,
        .action_kind = static_cast<std::uint16_t>(network::NetActionKind::UseTool),
        .source_entity_id = 11,
        .velocity_x = 4.0F,
        .velocity_y = -4.0F,
        .tool_slot = 1,
    });
    packet.messages.push_back(network::ActionRequestMessageEntry{
        .message_id = 2,
        .source_player_id = 2,
        .stage_instance_id = 3,
        .source_local_frame = 5,
        .action_kind = static_cast<std::uint16_t>(network::NetActionKind::HitEntity),
        .damage_type = static_cast<std::uint16_t>(DamageType::Attack),
        .projectile_contact_damage_type = static_cast<std::uint16_t>(DamageType::Attack),
        .flags = network::kActionRequestFlagClearVelocity |
                 network::kActionRequestFlagClearAcceleration,
        .source_entity_id = 11,
        .target_entity_id = 12,
        .velocity_x = 1.0F,
        .velocity_y = -1.0F,
        .amount = 1,
        .projectile_contact_damage_amount = 1,
        .thrown_immunity_timer = 12,
        .projectile_contact_duration = 20,
    });

    const network::EncodedNetPacket encoded = network::EncodeActionRequestMessages(packet);
    const std::size_t old_fixed_size =
        sizeof(network::NetPacketHeader) +
        sizeof(std::uint32_t) +
        packet.messages.size() * sizeof(network::ActionRequestMessageEntry);
    if (encoded.size == 0 || encoded.size >= old_fixed_size) {
        std::cerr << "network protocol smoke failed: compact action packet size "
                  << encoded.size << " did not improve on fixed size "
                  << old_fixed_size << '\n';
        return false;
    }

    const std::optional<network::ActionRequestMessagesPacket> decoded =
        network::TryDecodeActionRequestMessages(encoded.bytes.data(), encoded.size);
    if (!decoded.has_value() || decoded->messages.size() != packet.messages.size()) {
        std::cerr << "network protocol smoke failed: compact action packet did not round trip\n";
        return false;
    }
    return true;
}

} // namespace

bool CheckNetworkProtocolApplySmoke() {
    try {
        if (!CheckCompactActionRequestPacketSmoke()) {
            return false;
        }

        Graphics graphics;
        InitNetworkSmokeRuntimeTables(graphics);

        constexpr std::uint32_t seed = 12345;
        State coordinator = State::New();
        State peer = State::New();
        ConfigureProtocolSmokeCoordinator(coordinator);
        ConfigureProtocolSmokePeer(peer);

        if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "network protocol smoke failed: could not load test stages\n";
            return false;
        }
        ConfigureProtocolSmokeCoordinator(coordinator);
        ConfigureProtocolSmokePeer(peer);

        if (!CompareProtocolSmokeStates(coordinator, peer, "protocol after load")) {
            return false;
        }

        const Entity* source = FindFirstActiveEntity(coordinator);
        if (source == nullptr) {
            std::cerr << "network protocol smoke failed: no source entity\n";
            return false;
        }

        if (!world_ops::SetForegroundTile(coordinator, IVec2::New(3, 3), Tile::Rope)) {
            std::cerr << "network protocol smoke failed: coordinator did not set tile\n";
            return false;
        }
        if (!ApplyCoordinatorMessagesToPeer(coordinator, peer, "tile changed") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol tile changed")) {
            return false;
        }

        if (!world_ops::PlaceRopeTile(coordinator, *source, IVec2::New(4, 3))) {
            std::cerr << "network protocol smoke failed: coordinator did not place rope tile\n";
            return false;
        }
        if (!ApplyCoordinatorMessagesToPeer(coordinator, peer, "rope tile changed") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol rope tile changed")) {
            return false;
        }

        Entity* rock = world_ops::SpawnEntity(
            coordinator,
            EntityType::Rock,
            [](Entity& entity) {
                entity.pos = Vec2::New(96.0F, 64.0F);
                entity.vel = Vec2::New(1.0F, -2.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (rock == nullptr) {
            std::cerr << "network protocol smoke failed: coordinator did not spawn rock\n";
            return false;
        }
        if (!ApplyCoordinatorMessagesToPeer(coordinator, peer, "entity spawned") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol entity spawned")) {
            return false;
        }

        rock->vel = Vec2::New(2.0F, -1.0F);
        rock->money = 321;
        rock->stage_exit_id = 7;
        rock->holding = true;
        rock->render_enabled = false;
        rock->damage_vulnerability = DamageVulnerability::ExplosionOnly;
        world_ops::PatchEntityState(coordinator, *rock, *rock);
        if (!ApplyCoordinatorMessagesToPeer(coordinator, peer, "entity state patched") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol entity state patched")) {
            return false;
        }

        if (!world_ops::DeactivateEntity(coordinator, rock->vid)) {
            std::cerr << "network protocol smoke failed: coordinator did not deactivate rock\n";
            return false;
        }
        if (!ApplyCoordinatorMessagesToPeer(coordinator, peer, "entity deactivated") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol entity deactivated")) {
            return false;
        }

        std::cout << "network protocol apply smoke ok\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "network protocol smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks
