#include "step_entities.hpp"

#include "entities/altar.hpp"
#include "entities/arrow_trap.hpp"
#include "entities/baseball_bat.hpp"
#include "entities/bat.hpp"
#include "entities/block.hpp"
#include "entities/bomb.hpp"
#include "entities/breakaway_container.hpp"
#include "entities/caveman.hpp"
#include "entities/chest.hpp"
#include "entities/common.hpp"
#include "entities/damsel.hpp"
#include "entities/dice.hpp"
#include "entities/emerald_big.hpp"
#include "entities/gear_items.hpp"
#include "entities/ghost_ball.hpp"
#include "entities/giant_tiki_head.hpp"
#include "entities/gold_idol.hpp"
#include "entities/jetpack.hpp"
#include "entities/lantern.hpp"
#include "entities/mattock.hpp"
#include "entities/money.hpp"
#include "entities/mouse_trailer.hpp"
#include "entities/player.hpp"
#include "entities/rock.hpp"
#include "entities/rope.hpp"
#include "entities/ruby_big.hpp"
#include "entities/sac_altar.hpp"
#include "entities/scarab.hpp"
#include "entities/shopkeeper.hpp"
#include "entities/sign.hpp"
#include "entities/sapphire_big.hpp"
#include "entities/snake.hpp"
#include "entities/spider_hang.hpp"
#include "entities/stomp_pad.hpp"

namespace splonks {

/** Step the logic of entities, followed by their physics.  */
/*  Stepping Entities:
        Your entity must implement the following:
            step_logic_<entity_name>(game, map):
                - for logic
                - state machine stuff
                - other entity following
                - actions
            step_physics_<entity_name>(base_entity, game, map):
                - for motion
            step_animation_<entity_name>(base entity, game, map):
                - for stepping the animation state machine basically (can have common impls too)
        If several of your entities end up internally sharing a mutual implementation of some feature,
        you dont need a match out here or in step, just use the function which allows for that feature internally.
        compiler-san will make sure you have the necessary fields if you pass yourself into say,
            flying_entities_physics_step(*self)). because if you dont, compiler: reee
        You may also need to check if an entity has some trait, and that can go in entity, and instead of having a table you can just
        force every entity to implement is_burnable() for example. and the dyn table will take care of that for you.
*/
void StepEntities(State& state, Audio& audio, Graphics& graphics, float dt) {
    // extract and step the player first, this is so held items and stuff are updated with the latest player pos.
    // also makes sense to do this first, since the player is the most important entity.

    if (state.player_vid) {
        if (const Entity* const player = state.entity_manager.GetEntity(*state.player_vid)) {
            const bool has_physics = player->has_physics;
            const bool active = player->active;
            if (active) {
                entities::common::CommonStep(state.player_vid->id, state, graphics, audio, dt);
                entities::player::StepEntityLogicAsPlayer(
                    state.player_vid->id,
                    state,
                    graphics,
                    audio,
                    dt
                );
                entities::common::CommonPostStep(state.player_vid->id, state, graphics, audio, dt);
            }
            if (has_physics) {
                entities::player::StepEntityPhysicsAsPlayer(
                    state.player_vid->id,
                    state,
                    graphics,
                    audio,
                    dt
                );
            }
            entities::common::ApplyDeactivateConditions(state.player_vid->id, state);
            state.UpdateSidForEntity(state.player_vid->id, graphics);
        }
    }

    for (std::size_t entity_idx = 0; entity_idx < state.entity_manager.entities.size(); ++entity_idx) {
        const Entity& entity = state.entity_manager.GetEntityById(entity_idx);
        const EntityType type_ = entity.type_;
        const bool has_physics = entity.has_physics;

        if (entity.active) {
            if (type_ == EntityType::Player) {
                continue;
            }
            entities::common::CommonStep(entity_idx, state, graphics, audio, dt);
            switch (type_) {
            case EntityType::None:
            case EntityType::Player:
            case EntityType::GhostBall:
                entities::ghost_ball::StepEntityLogicAsGhostBall(entity_idx, state);
                break;
            case EntityType::Bat:
                entities::bat::StepEntityLogicAsBat(entity_idx, state, audio);
                break;
            case EntityType::Rock:
                entities::rock::StepEntityLogicAsRock(entity_idx, state, audio);
                break;
            case EntityType::Pot:
            case EntityType::Box:
                entities::breakaway_container::StepEntityLogicAsBreakawayContainer(
                    entity_idx, state, audio);
                break;
            case EntityType::Block:
                entities::block::StepEntityLogicAsBlock(entity_idx, state, audio);
                break;
            case EntityType::Bomb:
                entities::bomb::StepEntityLogicAsBomb(entity_idx, state, audio);
                break;
            case EntityType::JetPack:
                entities::jetpack::StepEntityLogicAsJetpack(entity_idx, state, audio);
                break;
            case EntityType::Rope:
                entities::rope::StepEntityLogicAsRope(entity_idx, state, audio, graphics);
                break;
            case EntityType::BaseballBat:
                entities::baseball_bat::StepBaseballBat(entity_idx, state, graphics, audio);
                break;
            case EntityType::MouseTrailer:
                entities::mouse_trailer::StepEntityLogicAsMouseTrailer(entity_idx, state, audio);
                break;
            case EntityType::Gold:
            case EntityType::GoldStack:
                entities::money::StepEntityLogicAsMoney(entity_idx, state, audio);
                break;
            case EntityType::StompPad:
                entities::stomp_pad::StepEntityLogicAsStompPad(entity_idx, state, audio);
                break;
            case EntityType::AltarLeft:
            case EntityType::AltarRight:
                entities::altar::StepEntityLogicAsAltar(entity_idx, state, audio);
                break;
            case EntityType::SacAltarLeft:
            case EntityType::SacAltarRight:
                entities::sac_altar::StepEntityLogicAsSacAltar(entity_idx, state, audio);
                break;
            case EntityType::GoldIdol:
                entities::gold_idol::StepEntityLogicAsGoldIdol(entity_idx, state, audio);
                break;
            case EntityType::Chest:
                entities::chest::StepEntityLogicAsChest(entity_idx, state, audio);
                break;
            case EntityType::Cape:
            case EntityType::Shotgun:
            case EntityType::Teleporter:
            case EntityType::Gloves:
            case EntityType::Spectacles:
            case EntityType::WebCannon:
            case EntityType::Pistol:
            case EntityType::Mitt:
            case EntityType::Paste:
            case EntityType::SpringShoes:
            case EntityType::SpikeShoes:
            case EntityType::Machete:
            case EntityType::BombBox:
            case EntityType::Bow:
            case EntityType::Compass:
            case EntityType::Parachute:
            case EntityType::RopePile:
                entities::gear_items::StepEntityLogicAsGearItem(entity_idx, state, audio);
                break;
            case EntityType::Mattock:
                entities::mattock::StepEntityLogicAsMattock(entity_idx, state, audio);
                break;
            case EntityType::Dice:
                entities::dice::StepEntityLogicAsDice(entity_idx, state, audio);
                break;
            case EntityType::RubyBig:
                entities::ruby_big::StepEntityLogicAsRubyBig(entity_idx, state, audio);
                break;
            case EntityType::EmeraldBig:
                entities::emerald_big::StepEntityLogicAsEmeraldBig(entity_idx, state, audio);
                break;
            case EntityType::SapphireBig:
                entities::sapphire_big::StepEntityLogicAsSapphireBig(entity_idx, state, audio);
                break;
            case EntityType::Shopkeeper:
                entities::shopkeeper::StepEntityLogicAsShopkeeper(entity_idx, state, audio);
                break;
            case EntityType::Damsel:
                entities::damsel::StepEntityLogicAsDamsel(entity_idx, state, audio);
                break;
            case EntityType::SignGeneral:
            case EntityType::SignBomb:
            case EntityType::SignWeapon:
            case EntityType::SignRare:
            case EntityType::SignClothing:
            case EntityType::SignCraps:
            case EntityType::SignKissing:
                entities::sign::StepEntityLogicAsSign(entity_idx, state, audio);
                break;
            case EntityType::Lantern:
            case EntityType::LanternRed:
                entities::lantern::StepEntityLogicAsLantern(entity_idx, state, audio);
                break;
            case EntityType::GiantTikiHead:
                entities::giant_tiki_head::StepEntityLogicAsGiantTikiHead(entity_idx, state, audio);
                break;
            case EntityType::ArrowTrap:
                entities::arrow_trap::StepEntityLogicAsArrowTrap(entity_idx, state, audio);
                break;
            case EntityType::Snake:
                entities::snake::StepEntityLogicAsSnake(entity_idx, state, audio);
                break;
            case EntityType::Caveman:
                entities::caveman::StepEntityLogicAsCaveman(entity_idx, state, audio);
                break;
            case EntityType::SpiderHang:
            case EntityType::GiantSpiderHang:
                entities::spider_hang::StepEntityLogicAsSpiderHang(entity_idx, state, audio);
                break;
            case EntityType::Scarab:
                entities::scarab::StepEntityLogicAsScarab(entity_idx, state, audio);
                break;
            }
            entities::common::CommonPostStep(entity_idx, state, graphics, audio, dt);

            if (has_physics) {
                switch (type_) {
                case EntityType::None:
                case EntityType::Player:
                case EntityType::GhostBall:
                    entities::ghost_ball::StepEntityPhysicsAsGhostBall(entity_idx, state, dt);
                    break;
                case EntityType::Bat:
                    entities::bat::StepEntityPhysicsAsBat(entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Rock:
                    entities::rock::StepEntityPhysicsAsRock(entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Pot:
                case EntityType::Box:
                    entities::breakaway_container::StepEntityPhysicsAsBreakawayContainer(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Block:
                    entities::block::StepEntityPhysicsAsBlock(entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Bomb:
                    entities::bomb::StepEntityPhysicsAsBomb(entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::JetPack:
                    entities::jetpack::StepEntityPhysicsAsJetpack(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Rope:
                    entities::rope::StepEntityPhysicsAsRope(entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::BaseballBat:
                    entities::baseball_bat::StepEntityPhysicsAsBaseballBat(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::MouseTrailer:
                    entities::mouse_trailer::StepEntityPhysicsAsMouseTrailer(entity_idx, state, dt);
                    break;
                case EntityType::Gold:
                case EntityType::GoldStack:
                    entities::money::StepEntityPhysicsAsMoney(entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::StompPad:
                    entities::stomp_pad::StepEntityPhysicsAsStompPad(
                        entity_idx,
                        state,
                        graphics,
                        audio,
                        dt
                    );
                    break;
                case EntityType::AltarLeft:
                case EntityType::AltarRight:
                case EntityType::SacAltarLeft:
                case EntityType::SacAltarRight:
                case EntityType::SignGeneral:
                case EntityType::SignBomb:
                case EntityType::SignWeapon:
                case EntityType::SignRare:
                case EntityType::SignClothing:
                case EntityType::SignCraps:
                case EntityType::SignKissing:
                case EntityType::Lantern:
                case EntityType::LanternRed:
                case EntityType::GiantTikiHead:
                case EntityType::ArrowTrap:
                    break;
                case EntityType::GoldIdol:
                    entities::gold_idol::StepEntityPhysicsAsGoldIdol(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Chest:
                    entities::chest::StepEntityPhysicsAsChest(entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Cape:
                case EntityType::Shotgun:
                case EntityType::Teleporter:
                case EntityType::Gloves:
                case EntityType::Spectacles:
                case EntityType::WebCannon:
                case EntityType::Pistol:
                case EntityType::Mitt:
                case EntityType::Paste:
                case EntityType::SpringShoes:
                case EntityType::SpikeShoes:
                case EntityType::Machete:
                case EntityType::BombBox:
                case EntityType::Bow:
                case EntityType::Compass:
                case EntityType::Parachute:
                case EntityType::RopePile:
                    entities::gear_items::StepEntityPhysicsAsGearItem(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Mattock:
                    entities::mattock::StepEntityPhysicsAsMattock(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Dice:
                    entities::dice::StepEntityPhysicsAsDice(entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::RubyBig:
                    entities::ruby_big::StepEntityPhysicsAsRubyBig(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::EmeraldBig:
                    entities::emerald_big::StepEntityPhysicsAsEmeraldBig(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::SapphireBig:
                    entities::sapphire_big::StepEntityPhysicsAsSapphireBig(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Shopkeeper:
                    entities::shopkeeper::StepEntityPhysicsAsShopkeeper(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Damsel:
                    entities::damsel::StepEntityPhysicsAsDamsel(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Snake:
                    entities::snake::StepEntityPhysicsAsSnake(entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Caveman:
                    entities::caveman::StepEntityPhysicsAsCaveman(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::SpiderHang:
                case EntityType::GiantSpiderHang:
                    entities::spider_hang::StepEntityPhysicsAsSpiderHang(
                        entity_idx, state, graphics, audio, dt);
                    break;
                case EntityType::Scarab:
                    entities::scarab::StepEntityPhysicsAsScarab(
                        entity_idx, state, graphics, audio, dt);
                    break;
                }
            }
            entities::common::ApplyDeactivateConditions(entity_idx, state);
            state.UpdateSidForEntity(entity_idx, graphics);
        }
    }
}

} // namespace splonks
