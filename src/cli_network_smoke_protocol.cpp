#include "cli_network_smoke.hpp"

#include "cli_network_smoke_internal.hpp"
#include "quest_stage_loader.hpp"

#include <exception>
#include <iostream>

namespace splonks {

bool CheckNetworkProtocolApplySmoke() {
    try {
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
        if (!ApplyCoordinatorEventsToPeer(coordinator, peer, "tile changed") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol tile changed")) {
            return false;
        }

        if (!world_ops::PlaceRopeTile(coordinator, *source, IVec2::New(4, 3))) {
            std::cerr << "network protocol smoke failed: coordinator did not place rope tile\n";
            return false;
        }
        if (!ApplyCoordinatorEventsToPeer(coordinator, peer, "rope tile changed") ||
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
        if (!ApplyCoordinatorEventsToPeer(coordinator, peer, "entity spawned") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol entity spawned")) {
            return false;
        }

        rock->vel = Vec2::New(2.0F, -1.0F);
        world_ops::PatchEntityState(coordinator, *rock, *rock);
        if (!ApplyCoordinatorEventsToPeer(coordinator, peer, "entity state patched") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol entity state patched")) {
            return false;
        }

        if (!world_ops::DeactivateEntity(coordinator, rock->vid)) {
            std::cerr << "network protocol smoke failed: coordinator did not deactivate rock\n";
            return false;
        }
        if (!ApplyCoordinatorEventsToPeer(coordinator, peer, "entity deactivated") ||
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
