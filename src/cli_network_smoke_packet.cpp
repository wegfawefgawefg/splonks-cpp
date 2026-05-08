#include "cli_network_smoke.hpp"

#include "cli_network_smoke_internal.hpp"
#include "network/net_lobby_internal.hpp"
#include "quest_stage_loader.hpp"

#include <exception>
#include <iostream>
#include <optional>

namespace splonks {

bool CheckNetworkPacketSmoke() {
    try {
        Graphics graphics;
        InitNetworkSmokeRuntimeTables(graphics);
        Audio audio;

        constexpr std::uint32_t seed = 12345;
        State coordinator = State::New();
        State peer = State::New();
        ConfigureProtocolSmokeCoordinator(coordinator);
        ConfigureProtocolSmokePeer(peer);

        if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "network packet smoke failed: could not load test stages\n";
            return false;
        }
        ConfigureProtocolSmokeCoordinator(coordinator);
        ConfigureProtocolSmokePeer(peer);
        LinkMatchingEntitiesForActionSmoke(coordinator, peer);

        network::NetTransportRuntime coordinator_transport = network::NetTransportRuntime::New();
        network::NetTransportRuntime peer_transport = network::NetTransportRuntime::New();
        coordinator_transport.capture_outgoing_packets = true;
        peer_transport.capture_outgoing_packets = true;
        const network::NetEndpoint coordinator_endpoint{
            .address = "127.0.0.1",
            .port = 43101,
        };
        const network::NetEndpoint peer_endpoint{
            .address = "127.0.0.1",
            .port = 43102,
        };
        peer_transport.coordinator_endpoint = coordinator_endpoint;
        coordinator_transport.remotes.push_back(network::NetRemoteEndpoint{
            .player_ids = {2},
            .endpoint = peer_endpoint,
            .last_heard_frame = 0,
            .highest_acked_coordinator_order = 0,
        });

        if (!CompareProtocolSmokeStates(coordinator, peer, "packet after load")) {
            return false;
        }

        const Entity* const coordinator_source = FindFirstPlayerLikeEntity(coordinator);
        const Entity* const peer_source = FindFirstPlayerLikeEntity(peer);
        if (coordinator_source == nullptr || peer_source == nullptr) {
            std::cerr << "network packet smoke failed: missing player-like source entity\n";
            return false;
        }
        const VID coordinator_source_vid = coordinator_source->vid;
        const VID peer_source_vid = peer_source->vid;
        const Vec2 coordinator_source_pos = coordinator_source->pos;

        const IVec2 break_tile_pos = IVec2::New(3, 3);
        (void)world_ops::SetForegroundTile(coordinator, break_tile_pos, Tile::CaveDirt);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup break tile"
            )) {
            return false;
        }

        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::BreakTile,
                    .source_vid = peer_source_vid,
                    .tile_pos = break_tile_pos,
                },
                graphics,
                audio,
                "packet break tile"
            )) {
            return false;
        }

        Entity* gold = world_ops::SpawnEntity(
            coordinator,
            EntityType::Gold,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos;
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (gold == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn gold\n";
            return false;
        }
        const VID coordinator_gold_vid = gold->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup collect"
            )) {
            return false;
        }
        const std::optional<VID> peer_gold_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_gold_vid);
        if (!peer_gold_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve spawned gold\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::CollectEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_gold_vid,
                },
                graphics,
                audio,
                "packet collect entity"
            )) {
            return false;
        }

        Entity* chest = world_ops::SpawnEntity(
            coordinator,
            EntityType::Chest,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos;
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (chest == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn chest\n";
            return false;
        }
        const VID coordinator_chest_vid = chest->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup interact chest"
            )) {
            return false;
        }
        const std::optional<VID> peer_chest_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_chest_vid);
        if (!peer_chest_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve spawned chest\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::InteractEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_chest_vid,
                },
                graphics,
                audio,
                "packet interact chest"
            )) {
            return false;
        }

        Entity* rock = world_ops::SpawnEntity(
            coordinator,
            EntityType::Rock,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(8.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (rock == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn rock\n";
            return false;
        }
        const VID coordinator_rock_vid = rock->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup pickup"
            )) {
            return false;
        }
        const std::optional<VID> peer_rock_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_rock_vid);
        if (!peer_rock_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve spawned rock\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PickupEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_rock_vid,
                },
                graphics,
                audio,
                "packet pickup entity"
            )) {
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::ThrowEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_rock_vid,
                    .velocity = Vec2::New(2.0F, -3.0F),
                },
                graphics,
                audio,
                "packet throw entity"
            )) {
            return false;
        }

        Entity* cape = world_ops::SpawnEntity(
            coordinator,
            EntityType::Cape,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(8.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (cape == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn cape\n";
            return false;
        }
        const VID coordinator_cape_vid = cape->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup cape"
            )) {
            return false;
        }
        const std::optional<VID> peer_cape_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_cape_vid);
        if (!peer_cape_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve spawned cape\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PickupEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_cape_vid,
                },
                graphics,
                audio,
                "packet pickup cape"
            )) {
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PutHeldEntityOnBack,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_cape_vid,
                },
                graphics,
                audio,
                "packet put cape on back"
            )) {
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::TakeOffBackEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_cape_vid,
                },
                graphics,
                audio,
                "packet take off cape"
            )) {
            return false;
        }
        Entity* drop_item = world_ops::SpawnEntity(
            coordinator,
            EntityType::Rock,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(8.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (drop_item == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn drop item\n";
            return false;
        }
        const VID coordinator_drop_item_vid = drop_item->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup drop"
            )) {
            return false;
        }
        const std::optional<VID> peer_drop_item_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_drop_item_vid);
        if (!peer_drop_item_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve drop item\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PickupEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_drop_item_vid,
                },
                graphics,
                audio,
                "packet pickup drop item"
            )) {
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::DropEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_drop_item_vid,
                },
                graphics,
                audio,
                "packet drop entity"
            )) {
            return false;
        }

        Entity* block = world_ops::SpawnEntity(
            coordinator,
            EntityType::Block,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(5.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (block == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn block\n";
            return false;
        }
        const VID coordinator_block_vid = block->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup push"
            )) {
            return false;
        }
        const std::optional<VID> peer_block_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_block_vid);
        if (!peer_block_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve spawned block\n";
            return false;
        }
        if (Entity* const source = coordinator.entity_manager.GetEntityMut(coordinator_source_vid)) {
            source->grounded = true;
        }
        if (Entity* const source = peer.entity_manager.GetEntityMut(peer_source_vid)) {
            source->grounded = true;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PushEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_block_vid,
                    .velocity = Vec2::New(0.5F, 0.0F),
                },
                graphics,
                audio,
                "packet push block"
            )) {
            return false;
        }

        Entity* damage_target = world_ops::SpawnEntity(
            coordinator,
            EntityType::Caveman,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(20.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (damage_target == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn damage target\n";
            return false;
        }
        const VID coordinator_damage_target_vid = damage_target->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup damage"
            )) {
            return false;
        }
        const std::optional<VID> peer_damage_target_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_damage_target_vid);
        if (!peer_damage_target_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve damage target\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::DamageEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_damage_target_vid,
                    .damage_type = DamageType::Attack,
                    .amount = 1,
                },
                graphics,
                audio,
                "packet damage entity"
            )) {
            return false;
        }

        Entity* hit_target = world_ops::SpawnEntity(
            coordinator,
            EntityType::Caveman,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(28.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (hit_target == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn hit target\n";
            return false;
        }
        const VID coordinator_hit_target_vid = hit_target->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup hit"
            )) {
            return false;
        }
        const std::optional<VID> peer_hit_target_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_hit_target_vid);
        if (!peer_hit_target_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve hit target\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::HitEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_hit_target_vid,
                    .velocity = Vec2::New(1.5F, -1.0F),
                    .damage_type = DamageType::Attack,
                    .projectile_contact_damage_type = DamageType::Attack,
                    .amount = 1,
                    .projectile_contact_damage_amount = 1,
                    .thrown_immunity_timer = 12,
                    .projectile_contact_duration = 20,
                    .clear_velocity = true,
                    .clear_acceleration = true,
                },
                graphics,
                audio,
                "packet hit entity"
            )) {
            return false;
        }

        const std::optional<std::size_t> peer_tool_slot =
            FindFirstUsableToolSlot(peer, peer_source_vid);
        if (!peer_tool_slot.has_value()) {
            std::cerr << "network packet smoke failed: peer has no usable tool slot\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::UseTool,
                    .source_vid = peer_source_vid,
                    .velocity = Vec2::New(4.0F, -4.0F),
                    .param_a = static_cast<std::uint32_t>(*peer_tool_slot),
                },
                graphics,
                audio,
                "packet use tool"
            )) {
            return false;
        }

        std::cout << "network packet smoke ok\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "network packet smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks
