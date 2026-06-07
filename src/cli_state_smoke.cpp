#include "cli_state_smoke.hpp"

#include "content_compat.hpp"
#include "debug/shop_test_stage.hpp"
#include "ent.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "effects.hpp"
#include "aframe.hpp"
#include "graphics.hpp"
#include "inputs.hpp"
#include "math_types.hpp"
#include "network/input_lockstep.hpp"
#include "network/net_ent_links.hpp"
#include "network/net_fuzzer.hpp"
#include "network/net_lobby_internal.hpp"
#include "network/net_lobby.hpp"
#include "quest_stage_loader.hpp"
#include "raw_aframe.hpp"
#include "sim/fxp.hpp"
#include "simulation_snapshot.hpp"
#include "stage_spawning.hpp"
#include "stage_progression.hpp"
#include "state.hpp"
#include "state_fingerprint.hpp"
#include "step.hpp"
#include "tile_source_data.hpp"
#include "tools/tool_spec.hpp"
#include "world_ops.hpp"

#include <array>
#include <algorithm>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace splonks {

namespace {

constexpr const char* kAnnotationsYamlPath = "assets/graphics/annotations.yaml";

void InitCliSmokeRuntimeTables(Graphics& graphics) {
    const RawAFrameFile raw_file = LoadRawAFrameFile(kAnnotationsYamlPath);
    graphics.aframe_db = AFrameDb::FromRaw(raw_file);
    graphics.tile_source_db = BuildTileSourceDb(graphics.aframe_db);

    PopulateEntSpecsTable();
    SyncEntSpecSizesFromAFrame(graphics);
    PopulateToolSpecsTable();
}

bool CompareCanonicalFingerprints(
    const State& left,
    const State& right,
    const char* label
) {
    const CanonicalStateFingerprint left_fingerprint = ComputeCanonicalStateFingerprint(left);
    const CanonicalStateFingerprint right_fingerprint = ComputeCanonicalStateFingerprint(right);
    if (left_fingerprint.value == right_fingerprint.value) {
        std::cout << "state equality smoke " << label << " ok: "
                  << left_fingerprint.summary
                  << " hash=" << left_fingerprint.value << '\n';
        return true;
    }

    std::cerr << "state equality smoke failed at " << label << "\n"
              << "  left  " << left_fingerprint.summary << " hash="
              << left_fingerprint.value << "\n"
              << "  right " << right_fingerprint.summary << " hash="
              << right_fingerprint.value << "\n";
    return false;
}

bool EntEffectsEqual(const BoxedEntEffects& left, const BoxedEntEffects& right) {
    const EntEffects* const a = left.get();
    const EntEffects* const b = right.get();
    if ((a == nullptr) != (b == nullptr)) {
        return false;
    }
    if (a == nullptr) {
        return true;
    }
    if (a->count != b->count) {
        return false;
    }
    for (std::uint8_t i = 0; i < a->count && i < a->effects.size(); ++i) {
        const EffectInstance& effect_a = a->effects[i];
        const EffectInstance& effect_b = b->effects[i];
        if (effect_a.id != effect_b.id ||
            effect_a.count != effect_b.count ||
            effect_a.value != effect_b.value ||
            effect_a.frames_remaining != effect_b.frames_remaining) {
            return false;
        }
    }
    return true;
}

std::string DescribeEntEffects(const BoxedEntEffects& effects_box) {
    const EntEffects* const effects = effects_box.get();
    if (effects == nullptr) {
        return "none";
    }
    std::ostringstream output;
    output << "count=" << static_cast<int>(effects->count);
    for (std::uint8_t i = 0; i < effects->count && i < effects->effects.size(); ++i) {
        const EffectInstance& effect = effects->effects[i];
        output << " [" << i
               << " id=" << static_cast<int>(effect.id)
               << " count=" << effect.count
               << " value=" << sim::ToRenderScalar(effect.value)
               << " frames=" << effect.frames_remaining
               << "]";
    }
    return output.str();
}

std::string DescribeFirstStateDifference(const State& left, const State& right) {
    if (left.stage.GetStageDims() != right.stage.GetStageDims()) {
        std::ostringstream output;
        output << "stage dims differ: left=" << left.stage.GetTileWidth() << "x"
               << left.stage.GetTileHeight() << " right=" << right.stage.GetTileWidth()
               << "x" << right.stage.GetTileHeight();
        return output.str();
    }

    for (unsigned int y = 0; y < left.stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < left.stage.GetTileWidth(); ++x) {
            if (left.stage.GetTile(x, y) != right.stage.GetTile(x, y)) {
                std::ostringstream output;
                output << "foreground tile differs at " << x << "," << y
                       << ": left=" << static_cast<int>(left.stage.GetTile(x, y))
                       << " right=" << static_cast<int>(right.stage.GetTile(x, y));
                return output.str();
            }
            if (left.stage.GetBackwallTile(x, y) != right.stage.GetBackwallTile(x, y)) {
                std::ostringstream output;
                output << "backwall tile differs at " << x << "," << y
                       << ": left=" << static_cast<int>(left.stage.GetBackwallTile(x, y))
                       << " right=" << static_cast<int>(right.stage.GetBackwallTile(x, y));
                return output.str();
            }
        }
    }

    if (left.ents.ents.size() != right.ents.ents.size()) {
        std::ostringstream output;
        output << "ent array size differs: left=" << left.ents.ents.size()
               << " right=" << right.ents.ents.size();
        return output.str();
    }

    for (std::size_t i = 0; i < left.ents.ents.size(); ++i) {
        const Ent& a = left.ents.ents[i];
        const Ent& b = right.ents.ents[i];
        if (a.active != b.active ||
            a.type_ != b.type_ ||
            a.vid != b.vid) {
            std::ostringstream output;
            output << "ent " << i << " ident differs:"
                   << " frame " << left.frame << "/" << right.frame
                   << " stage_frame " << left.stage_frame << "/" << right.stage_frame
                   << " mode " << static_cast<int>(left.mode) << "/" << static_cast<int>(right.mode)
                   << " active " << a.active << "/" << b.active
                   << " type " << static_cast<int>(a.type_) << "/" << static_cast<int>(b.type_)
                   << " vid " << a.vid.id << ":" << a.vid.version
                   << "/" << b.vid.id << ":" << b.vid.version;
            std::size_t version_diff_count = 0;
            std::ostringstream version_examples;
            for (std::size_t j = 0; j < left.ents.ents.size(); ++j) {
                const Ent& left_ent = left.ents.ents[j];
                const Ent& right_ent = right.ents.ents[j];
                if (left_ent.vid.version != right_ent.vid.version) {
                    if (version_diff_count < 8) {
                        version_examples << " [" << j
                                         << " t" << static_cast<int>(left_ent.type_)
                                         << "/" << static_cast<int>(right_ent.type_)
                                         << " v" << left_ent.vid.version
                                         << "/" << right_ent.vid.version
                                         << " a" << left_ent.active
                                         << "/" << right_ent.active << "]";
                    }
                    version_diff_count += 1;
                }
            }
            output << " version_diffs=" << version_diff_count
                   << " available_ids " << left.ents.available_ids.size()
                   << "/" << right.ents.available_ids.size()
                   << version_examples.str();
            return output.str();
        }
        if (!a.active) {
            continue;
        }
        if (a.pos != b.pos ||
            a.vel != b.vel ||
            a.acc != b.acc ||
            a.has_physics != b.has_physics ||
            a.can_collide != b.can_collide ||
            a.grounded != b.grounded ||
            a.holding != b.holding ||
            a.wanted != b.wanted ||
            a.render_enabled != b.render_enabled ||
            a.size != b.size ||
            a.rotation != b.rotation ||
            a.coyote_time != b.coyote_time ||
            a.stun_timer != b.stun_timer ||
            a.fall_timer != b.fall_timer ||
            a.facing != b.facing ||
            a.draw_layer != b.draw_layer ||
            a.condition != b.condition ||
            a.ai_state != b.ai_state ||
            a.damage_vuln != b.damage_vuln ||
            a.movement_flags != b.movement_flags ||
            a.health != b.health ||
            a.max_speed != b.max_speed ||
            a.throw_velocity_scale != b.throw_velocity_scale ||
            a.buoyancy != b.buoyancy ||
            a.back_vid != b.back_vid ||
            a.holding_vid != b.holding_vid ||
            a.held_by_vid != b.held_by_vid ||
            a.ent_a != b.ent_a ||
            a.ent_b != b.ent_b ||
            a.ent_c != b.ent_c ||
            a.ent_d != b.ent_d ||
            a.stage_exit_id != b.stage_exit_id ||
            a.money != b.money ||
            a.counter_a != b.counter_a ||
            a.counter_b != b.counter_b ||
            a.counter_c != b.counter_c ||
            a.counter_d != b.counter_d ||
            a.self_light != b.self_light ||
            a.light_strength != b.light_strength ||
            a.light_color.r != b.light_color.r ||
            a.light_color.g != b.light_color.g ||
            a.light_color.b != b.light_color.b ||
            a.light_radius != b.light_radius ||
            a.point_a != b.point_a ||
            a.point_b != b.point_b ||
            a.point_c != b.point_c ||
            a.point_d != b.point_d ||
            a.aframe_animator.anim_id != b.aframe_animator.anim_id ||
            a.aframe_animator.current_frame != b.aframe_animator.current_frame ||
            a.aframe_animator.current_time != b.aframe_animator.current_time ||
            a.aframe_animator.scale != b.aframe_animator.scale ||
            a.aframe_animator.speed != b.aframe_animator.speed ||
            a.aframe_animator.animate != b.aframe_animator.animate ||
            a.aframe_animator.loop != b.aframe_animator.loop ||
            a.aframe_animator.finished != b.aframe_animator.finished ||
            !EntEffectsEqual(a.effects, b.effects)) {
            std::ostringstream output;
            output << "ent " << i << " differs:"
                   << " active " << a.active << "/" << b.active
                   << " type " << static_cast<int>(a.type_) << "/" << static_cast<int>(b.type_)
                   << " vidver " << a.vid.version << "/" << b.vid.version
                   << " physics " << a.has_physics << "/" << b.has_physics
                   << " collide " << a.can_collide << "/" << b.can_collide
                   << " pos " << a.pos.x << "," << a.pos.y
                   << "/" << b.pos.x << "," << b.pos.y
                   << " vel " << a.vel.x << "," << a.vel.y
                   << "/" << b.vel.x << "," << b.vel.y
                   << " acc " << a.acc.x << "," << a.acc.y
                   << "/" << b.acc.x << "," << b.acc.y
                   << " size " << a.size.x << "," << a.size.y
                   << "/" << b.size.x << "," << b.size.y
                   << " grounded " << a.grounded << "/" << b.grounded
                   << " holding " << a.holding << "/" << b.holding
                   << " wanted " << a.wanted << "/" << b.wanted
                   << " render " << a.render_enabled << "/" << b.render_enabled
                   << " rotation " << sim::ToRenderScalar(a.rotation) << "/"
                   << sim::ToRenderScalar(b.rotation)
                   << " coyote " << a.coyote_time << "/" << b.coyote_time
                   << " stun " << a.stun_timer << "/" << b.stun_timer
                   << " fall " << a.fall_timer << "/" << b.fall_timer
                   << " facing " << static_cast<int>(a.facing) << "/" << static_cast<int>(b.facing)
                   << " layer " << static_cast<int>(a.draw_layer) << "/" << static_cast<int>(b.draw_layer)
                   << " health " << a.health << "/" << b.health
                   << " condition " << static_cast<int>(a.condition)
                   << "/" << static_cast<int>(b.condition)
                   << " ai " << static_cast<int>(a.ai_state) << "/" << static_cast<int>(b.ai_state)
                   << " vuln " << static_cast<int>(a.damage_vuln)
                   << "/" << static_cast<int>(b.damage_vuln)
                   << " move_flags " << a.movement_flags << "/" << b.movement_flags
                   << " back " << (a.back_vid.has_value() ? static_cast<int>(a.back_vid->id) : -1)
                   << "/" << (b.back_vid.has_value() ? static_cast<int>(b.back_vid->id) : -1)
                   << " holding_vid "
                   << (a.holding_vid.has_value() ? static_cast<int>(a.holding_vid->id) : -1)
                   << "/" << (b.holding_vid.has_value() ? static_cast<int>(b.holding_vid->id) : -1)
                   << " held_by "
                   << (a.held_by_vid.has_value() ? static_cast<int>(a.held_by_vid->id) : -1)
                   << "/" << (b.held_by_vid.has_value() ? static_cast<int>(b.held_by_vid->id) : -1)
                   << " counters " << a.counter_a << "," << a.counter_b
                   << "," << a.counter_c << "," << a.counter_d
                   << "/" << b.counter_a << "," << b.counter_b
                   << "," << b.counter_c << "," << b.counter_d
                   << " lights " << sim::ToRenderScalar(a.light_strength) << ","
                   << a.light_radius << "/" << sim::ToRenderScalar(b.light_strength) << ","
                   << b.light_radius
                   << " points " << a.point_a.x << "," << a.point_a.y
                   << "/" << b.point_a.x << "," << b.point_a.y
                   << " anim " << a.aframe_animator.anim_id
                   << "/" << b.aframe_animator.anim_id
                   << " frame " << a.aframe_animator.current_frame
                   << "/" << b.aframe_animator.current_frame
                   << " time " << sim::ToRenderScalar(a.aframe_animator.current_time)
                   << "/" << sim::ToRenderScalar(b.aframe_animator.current_time)
                   << " effects " << DescribeEntEffects(a.effects)
                   << " / " << DescribeEntEffects(b.effects);
            return output.str();
        }
    }

    if (left.drng.state != right.drng.state) {
        std::ostringstream output;
        output << "drng differs: left=" << left.drng.state << " right=" << right.drng.state;
        return output.str();
    }
    if (left.stage.tile_change_generation != right.stage.tile_change_generation) {
        std::ostringstream output;
        output << "tile_change_generation differs: left=" << left.stage.tile_change_generation
               << " right=" << right.stage.tile_change_generation;
        return output.str();
    }

    if (left.players.slots.size() != right.players.slots.size()) {
        std::ostringstream output;
        output << "player slot count differs: left=" << left.players.slots.size()
               << " right=" << right.players.slots.size();
        return output.str();
    }
    for (std::size_t i = 0; i < left.players.slots.size(); ++i) {
        const PlayerSlot& a = left.players.slots[i];
        const PlayerSlot& b = right.players.slots[i];
        if (a.player_id != b.player_id ||
            a.connected != b.connected ||
            a.ent_vid != b.ent_vid ||
            a.input_frame.left != b.input_frame.left ||
            a.input_frame.right != b.input_frame.right ||
            a.input_frame.up != b.input_frame.up ||
            a.input_frame.down != b.input_frame.down ||
            a.input_frame.jump != b.input_frame.jump ||
            a.input_frame.run != b.input_frame.run ||
            a.input_frame.use_button != b.input_frame.use_button ||
            a.input_frame.equip_button != b.input_frame.equip_button ||
            a.input_frame.pick_up_drop != b.input_frame.pick_up_drop ||
            a.input_frame.bomb != b.input_frame.bomb ||
            a.input_frame.rope != b.input_frame.rope ||
            a.input_frame.attack != b.input_frame.attack ||
            a.previous_input_frame.left != b.previous_input_frame.left ||
            a.previous_input_frame.right != b.previous_input_frame.right ||
            a.previous_input_frame.up != b.previous_input_frame.up ||
            a.previous_input_frame.down != b.previous_input_frame.down ||
            a.previous_input_frame.jump != b.previous_input_frame.jump ||
            a.previous_input_frame.run != b.previous_input_frame.run ||
            a.previous_input_frame.use_button != b.previous_input_frame.use_button ||
            a.previous_input_frame.equip_button != b.previous_input_frame.equip_button ||
            a.previous_input_frame.pick_up_drop != b.previous_input_frame.pick_up_drop ||
            a.previous_input_frame.bomb != b.previous_input_frame.bomb ||
            a.previous_input_frame.rope != b.previous_input_frame.rope ||
            a.previous_input_frame.attack != b.previous_input_frame.attack) {
            std::ostringstream output;
            output << "player slot " << i << " differs:"
                   << " player_id " << a.player_id << "/" << b.player_id
                   << " connected " << a.connected << "/" << b.connected
                   << " ent_vid "
                   << (a.ent_vid.has_value() ? static_cast<int>(a.ent_vid->id) : -1)
                   << "/"
                   << (b.ent_vid.has_value() ? static_cast<int>(b.ent_vid->id) : -1)
                   << " input_flags " << network::PackInputFrame(a.input_frame)
                   << "/" << network::PackInputFrame(b.input_frame)
                   << " previous_flags " << network::PackInputFrame(a.previous_input_frame)
                   << "/" << network::PackInputFrame(b.previous_input_frame);
            return output.str();
        }
    }

    return "no simple lane difference found; fingerprint includes a field not covered by the smoke diff";
}

const Ent* FindFirstActiveEnt(const State& state) {
    for (const Ent& ent : state.ents.ents) {
        if (ent.active) {
            return &ent;
        }
    }
    return nullptr;
}

bool ApplyDetWorldOpsSmokeMutations(State& state, const char*& failed_step) {
    const Ent* source = FindFirstActiveEnt(state);
    if (source == nullptr) {
        failed_step = "find source ent";
        return false;
    }

    if (!world_ops::SetForegroundTile(state, IVec2::New(3, 3), Tile::Rope)) {
        failed_step = "set foreground tile";
        return false;
    }

    if (!world_ops::PlaceRopeTile(state, *source, IVec2::New(4, 3))) {
        failed_step = "place rope tile";
        return false;
    }

    Ent* rock = world_ops::SpawnEnt(
        state,
        EntType::Rock,
        [](Ent& ent) {
            ent.pos = Vec2::New(96.0F, 64.0F);
            ent.vel = Vec2::New(1.0F, -2.0F);
            ent.acc = Vec2::New(0.0F, 0.0F);
        }
    );
    if (rock == nullptr) {
        failed_step = "spawn rock";
        return false;
    }
    if (!world_ops::DeactivateEnt(state, rock->vid)) {
        failed_step = "deactivate rock";
        return false;
    }

    return true;
}

std::vector<InputFrame> BuildDetReplayInputScript() {
    std::vector<InputFrame> frames(180, InputFrame::New());
    for (std::size_t i = 0; i < frames.size(); ++i) {
        InputFrame& frame = frames[i];
        frame.mouse_pos = UVec2::New(320, 180);

        if (i < 45) {
            frame.right = true;
        } else if (i < 80) {
            frame.left = true;
        } else if (i < 130) {
            frame.right = true;
            frame.run = true;
        }

        if ((i >= 8 && i < 16) || (i >= 62 && i < 70) || (i >= 108 && i < 116)) {
            frame.jump = true;
        }
        if (i >= 132 && i < 150) {
            frame.down = true;
        }
    }
    return frames;
}

std::vector<std::array<InputFrame, 2>> BuildDetMultiLocalReplayInputScript() {
    std::vector<std::array<InputFrame, 2>> frames(240);
    for (std::size_t i = 0; i < frames.size(); ++i) {
        InputFrame p1 = InputFrame::New();
        InputFrame p2 = InputFrame::New();
        p1.mouse_pos = UVec2::New(320, 180);
        p2.mouse_pos = UVec2::New(320, 180);

        p1.right = i < 70 || (i >= 130 && i < 190);
        p1.left = i >= 80 && i < 118;
        p1.run = i >= 130;
        p1.jump = (i >= 10 && i < 16) || (i >= 92 && i < 98);

        p2.left = i < 55 || (i >= 150 && i < 205);
        p2.right = i >= 70 && i < 130;
        p2.run = i >= 150;
        p2.jump = (i >= 24 && i < 30) || (i >= 164 && i < 170);
        p2.down = i >= 210 && i < 225;

        frames[i] = {p1, p2};
    }
    return frames;
}

std::vector<InputFrame> BuildBroadDetReplayInputScript() {
    std::vector<InputFrame> frames(1000, InputFrame::New());
    for (std::size_t i = 0; i < frames.size(); ++i) {
        InputFrame& frame = frames[i];
        frame.mouse_pos = UVec2::New(340, 210);
        frame.right = (i >= 12 && i < 180) || (i >= 300 && i < 460) || (i >= 620 && i < 780);
        frame.left = (i >= 210 && i < 285) || (i >= 500 && i < 580);
        frame.run = (i >= 80 && i < 170) || (i >= 640 && i < 740);
        frame.jump = (i >= 32 && i < 40) || (i >= 140 && i < 148) ||
                     (i >= 360 && i < 368) || (i >= 700 && i < 708);
        frame.pick_up_drop = (i >= 4 && i < 8) || (i >= 56 && i < 60) || (i >= 430 && i < 434);
        frame.attack = (i >= 22 && i < 26) || (i >= 120 && i < 124) || (i >= 520 && i < 524);
        frame.bomb = i >= 190 && i < 194;
        frame.rope = i >= 250 && i < 254;
        frame.down = i >= 830 && i < 860;
    }
    return frames;
}

std::vector<InputFrame> BuildNeutralInputScript(std::size_t frame_count) {
    std::vector<InputFrame> frames(frame_count, InputFrame::New());
    for (InputFrame& frame : frames) {
        frame.mouse_pos = UVec2::New(320, 180);
    }
    return frames;
}

std::vector<InputFrame> BuildShopDetReplayInputScript() {
    std::vector<InputFrame> frames = BuildNeutralInputScript(90);
    frames[1].equip_button = true;
    return frames;
}

void ApplyPrimaryInputFrame(State& state, const InputFrame& input_frame) {
    state.playing_input_snapshot = ToPlayingInputSnapshot(input_frame);
}

Ent* FindPrimaryPlayerMut(State& state) {
    PlayerSlot* const primary = state.players.FindPrimaryLocal();
    if (primary == nullptr || !primary->ent_vid.has_value()) {
        return nullptr;
    }
    return state.ents.GetEntMut(*primary->ent_vid);
}

bool SetScenarioForegroundTile(State& state, const IVec2& tile_pos, Tile tile) {
    const IVec2 wrapped_tile_pos = state.stage.WrapTileCoord(tile_pos);
    if (!state.stage.IsTileCoordInside(wrapped_tile_pos.x, wrapped_tile_pos.y)) {
        return false;
    }
    if (state.stage.GetTile(
            static_cast<unsigned int>(wrapped_tile_pos.x),
            static_cast<unsigned int>(wrapped_tile_pos.y)
        ) == tile &&
        state.stage.GetTileRotation(
            static_cast<unsigned int>(wrapped_tile_pos.x),
            static_cast<unsigned int>(wrapped_tile_pos.y)
        ) == kTileRotation0) {
        return true;
    }
    return world_ops::SetForegroundTile(state, wrapped_tile_pos, tile);
}

bool PrepareBroadDetReplayScenario(State& state, const char*& failed_step) {
    Ent* const player = FindPrimaryPlayerMut(state);
    if (player == nullptr) {
        failed_step = "find primary player";
        return false;
    }

    for (int y = 8; y <= 20; ++y) {
        for (int x = 3; x <= 30; ++x) {
            const Tile tile = y == 20 ? Tile::CaveBlock : Tile::Air;
            if (!SetScenarioForegroundTile(state, IVec2::New(x, y), tile)) {
                failed_step = "prepare arena tiles";
                return false;
            }
        }
    }
    for (int y = 16; y <= 19; ++y) {
        if (!SetScenarioForegroundTile(state, IVec2::New(13, y), Tile::Ladder)) {
            failed_step = "prepare ladder";
            return false;
        }
    }
    if (!SetScenarioForegroundTile(state, IVec2::New(18, 19), Tile::Spikes)) {
        failed_step = "prepare spikes";
        return false;
    }

    player->pos = Vec2::New(4.0F * static_cast<float>(kTileSize), 20.0F * static_cast<float>(kTileSize) - player->size.y);
    player->vel = Vec2::New(0.0F, 0.0F);
    player->acc = Vec2::New(0.0F, 0.0F);
    player->grounded = false;
    player->condition = EntCondition::Normal;
    player->stun_timer = 0;
    FillToolSlot(state.ent_tools.EnsureToolSlot(player->vid, 0), ToolKind::ThrowBomb, 3, true);
    FillToolSlot(state.ent_tools.EnsureToolSlot(player->vid, 1), ToolKind::ThrowRope, 3, true);

    const auto spawn = [&](EntType type, Vec2 pos) -> bool {
        Ent* const ent = world_ops::SpawnEnt(state, type, [&](Ent& spawned) {
            spawned.pos = pos;
            spawned.vel = Vec2::New(0.0F, 0.0F);
            spawned.acc = Vec2::New(0.0F, 0.0F);
        });
        return ent != nullptr;
    };
    if (!spawn(EntType::Rock, Vec2::New(5.0F * static_cast<float>(kTileSize), 20.0F * static_cast<float>(kTileSize) - 8.0F)) ||
        !spawn(EntType::Pot, Vec2::New(9.0F * static_cast<float>(kTileSize), 19.0F * static_cast<float>(kTileSize))) ||
        !spawn(EntType::Box, Vec2::New(11.0F * static_cast<float>(kTileSize), 19.0F * static_cast<float>(kTileSize))) ||
        !spawn(EntType::Snake, Vec2::New(16.0F * static_cast<float>(kTileSize), 19.0F * static_cast<float>(kTileSize)))) {
        failed_step = "spawn broad scenario ents";
        return false;
    }

    return true;
}

bool PrepareFluidDetReplayScenario(State& state, const char*& failed_step) {
    if (!PrepareBroadDetReplayScenario(state, failed_step)) {
        return false;
    }
    Ent* const player = FindPrimaryPlayerMut(state);
    if (player == nullptr) {
        failed_step = "find fluid scenario player";
        return false;
    }

    state.settings.fluid.simulation_enabled = true;
    state.settings.fluid.simulation_interval_frames = 1;
    state.settings.fluid.transfer_per_step = 0.55F;
    state.settings.fluid.pressure_strength = 0.65F;
    state.settings.fluid.velocity_damping = 0.86F;
    state.settings.fluid.gravity_x = 0.0F;
    state.settings.fluid.gravity_y = 1.0F;

    for (int x = 6; x <= 18; ++x) {
        if (!SetScenarioForegroundTile(state, IVec2::New(x, 18), Tile::CaveBlock)) {
            failed_step = "prepare fluid basin floor";
            return false;
        }
    }
    for (int y = 13; y <= 18; ++y) {
        if (!SetScenarioForegroundTile(state, IVec2::New(6, y), Tile::CaveBlock) ||
            !SetScenarioForegroundTile(state, IVec2::New(18, y), Tile::CaveBlock)) {
            failed_step = "prepare fluid basin walls";
            return false;
        }
    }
    for (int y = 11; y <= 14; ++y) {
        for (int x = 8; x <= 13; ++x) {
            state.stage.SetFluidTile(IVec2::New(x, y), Tile::WaterSwim);
        }
    }
    state.stage.SetFluidGravityOverride(IVec2::New(9, 11), Vec2::New(0.35F, 1.0F));
    state.stage.AddFluidTempGravity(IVec2::New(12, 11), Vec2::New(1.25F, 0.0F));

    Ent* const box = world_ops::SpawnEnt(state, EntType::Box, [](Ent& ent) {
        ent.pos = Vec2::New(10.0F * static_cast<float>(kTileSize), 15.0F * static_cast<float>(kTileSize));
        ent.vel = Vec2::New(0.0F, 0.0F);
        ent.acc = Vec2::New(0.0F, 0.0F);
    });
    if (box == nullptr) {
        failed_step = "spawn fluid scenario box";
        return false;
    }

    player->pos = Vec2::New(
        22.0F * static_cast<float>(kTileSize),
        20.0F * static_cast<float>(kTileSize) - player->size.y
    );
    player->vel = Vec2::New(0.0F, 0.0F);
    player->acc = Vec2::New(0.0F, 0.0F);
    return true;
}

bool PrepareShopDetReplayScenario(State& state, const char*& failed_step) {
    state.stage = MakeShopTestStage();
    InitShopTestStage(state);
    for (unsigned int y = 0; y < state.stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < state.stage.GetTileWidth(); ++x) {
            state.stage.SetBackwallTile(
                IVec2::New(static_cast<int>(x), static_cast<int>(y)),
                Tile::CaveAir0
            );
        }
    }

    Ent* const player = FindPrimaryPlayerMut(state);
    if (player == nullptr) {
        failed_step = "find shop scenario player";
        return false;
    }
    player->money = 50000;
    player->pos = Vec2::New(
        16.0F * static_cast<float>(kTileSize),
        10.0F * static_cast<float>(kTileSize) - player->size.y
    );
    player->vel = Vec2::New(0.0F, 0.0F);
    player->acc = Vec2::New(0.0F, 0.0F);
    player->grounded = false;
    state.mode = Mode::Playing;
    return true;
}

bool AddSecondLocalPlayerForDetReplay(State& state, Graphics& graphics) {
    constexpr PlayerId kSecondPlayerId = 2;
    (void)state.players.EnsureLocalPlayer(kSecondPlayerId, "Player 2", false);

    Vec2 spawn_pos = Vec2::New(32.0F, 32.0F);
    if (const PlayerSlot* const primary = state.players.FindPrimaryLocal();
        primary != nullptr && primary->ent_vid.has_value()) {
        if (const Ent* const primary_ent = state.ents.GetEnt(*primary->ent_vid)) {
            spawn_pos = primary_ent->pos + Vec2::New(16.0F, 0.0F);
        }
    }

    const std::optional<VID> second_player_vid =
        SpawnPlayerForPlayerId(state, kSecondPlayerId, spawn_pos);
    if (!second_player_vid.has_value()) {
        return false;
    }
    state.UpdateSidForEnt(second_player_vid->id, graphics);
    return true;
}

void ApplyMultiLocalInputFrame(
    State& state,
    const std::array<InputFrame, 2>& input_frame
) {
    ApplyPrimaryInputFrame(state, input_frame[0]);
    state.players.SetInputFrameForPlayer(2, input_frame[1]);
}

bool CompareDetReplayFingerprints(
    const State& recorded,
    const State& replayed,
    const char* label,
    std::size_t frame_index
) {
    const CanonicalStateFingerprint recorded_fingerprint =
        ComputeGameplayDeterminismFingerprint(recorded);
    const CanonicalStateFingerprint replayed_fingerprint =
        ComputeGameplayDeterminismFingerprint(replayed);
    if (recorded_fingerprint.value == replayed_fingerprint.value) {
        return true;
    }
    std::cerr << label << " failed at frame "
              << frame_index << "\n"
              << "  recorded " << recorded_fingerprint.summary
              << " hash=" << recorded_fingerprint.value << "\n"
              << "  replayed " << replayed_fingerprint.summary
              << " hash=" << replayed_fingerprint.value << "\n"
              << "  first simple diff: "
              << DescribeFirstStateDifference(recorded, replayed) << '\n';
    return false;
}

bool RunSinglePlayerDetReplayScenario(
    State& recorded,
    State& replayed,
    Audio& audio,
    Graphics& graphics,
    const std::vector<InputFrame>& inputs,
    const char* label
) {
    if (!CompareCanonicalFingerprints(recorded, replayed, label)) {
        std::cerr << "  first simple diff: "
                  << DescribeFirstStateDifference(recorded, replayed) << '\n';
        return false;
    }

    for (std::size_t frame_index = 0; frame_index < inputs.size(); ++frame_index) {
        ApplyPrimaryInputFrame(recorded, inputs[frame_index]);
        ApplyPrimaryInputFrame(replayed, inputs[frame_index]);
        StepSingleTick(recorded, audio, graphics);
        StepSingleTick(replayed, audio, graphics);
        if (!CompareDetReplayFingerprints(recorded, replayed, label, frame_index)) {
            return false;
        }
    }

    const CanonicalStateFingerprint final_fingerprint =
        ComputeGameplayDeterminismFingerprint(recorded);
    std::cout << label << " ok: frames=" << inputs.size()
              << " " << final_fingerprint.summary
              << " hash=" << final_fingerprint.value << '\n';
    return true;
}

std::vector<std::array<InputFrame, 2>> BuildInputLockstepSmokeScript() {
    std::vector<std::array<InputFrame, 2>> frames(1200);
    const std::vector<InputFrame> broad = BuildBroadDetReplayInputScript();
    for (std::size_t i = 0; i < frames.size(); ++i) {
        InputFrame p1 = i < broad.size() ? broad[i] : InputFrame::New();
        InputFrame p2 = InputFrame::New();
        p2.mouse_pos = UVec2::New(300, 190);

        p2.left = (i >= 16 && i < 96) || (i >= 240 && i < 300);
        p2.right = (i >= 112 && i < 212) || (i >= 310 && i < 350);
        p2.run = i >= 150 && i < 220;
        p2.jump = (i >= 40 && i < 48) || (i >= 168 && i < 176);
        p2.pick_up_drop = (i >= 72 && i < 76) || (i >= 132 && i < 136);
        p2.attack = i >= 190 && i < 194;
        p2.down = i >= 300 && i < 322;

        frames[i] = {p1, p2};
    }
    return frames;
}

std::vector<std::array<InputFrame, 2>> BuildInputLockstepCarryScript() {
    std::vector<std::array<InputFrame, 2>> frames(180);
    for (std::size_t i = 0; i < frames.size(); ++i) {
        InputFrame p1 = InputFrame::New();
        InputFrame p2 = InputFrame::New();
        p1.mouse_pos = UVec2::New(220, 190);
        p2.mouse_pos = UVec2::New(240, 190);

        p2.right = i >= 8 && i < 110;
        p2.run = i >= 36 && i < 100;
        p2.jump = (i >= 64 && i < 70);
        p2.left = i >= 125 && i < 150;

        frames[i] = {p1, p2};
    }
    return frames;
}

std::optional<VID> FindPlayerVidForSmoke(State& state, PlayerId player_id) {
    PlayerSlot* const slot = state.players.Find(player_id);
    if (slot == nullptr || !slot->connected || !slot->ent_vid.has_value()) {
        return std::nullopt;
    }
    Ent* player = state.ents.GetEntMut(*slot->ent_vid);
    if (player == nullptr || !player->active) {
        return std::nullopt;
    }
    return player->vid;
}

bool PlaceCarryTransitionSmokePlayers(State& state, Graphics& graphics, const char*& failed_step) {
    const std::optional<VID> p1_vid = FindPlayerVidForSmoke(state, 1);
    const std::optional<VID> p2_vid = FindPlayerVidForSmoke(state, 2);
    if (!p1_vid.has_value() || !p2_vid.has_value()) {
        failed_step = "find carry smoke players";
        return false;
    }

    Ent* const p1 = state.ents.GetEntMut(*p1_vid);
    Ent* const p2 = state.ents.GetEntMut(*p2_vid);
    if (p1 == nullptr || p2 == nullptr) {
        failed_step = "resolve carry smoke players";
        return false;
    }

    const float tile = static_cast<float>(kTileSize);
    const float floor_y = 20.0F * tile;
    p1->pos = Vec2::New(8.0F * tile, floor_y - p1->size.y);
    p2->pos = Vec2::New(9.0F * tile, floor_y - p2->size.y);
    for (Ent* const player : {p1, p2}) {
        player->vel = Vec2::New(0.0F, 0.0F);
        player->acc = Vec2::New(0.0F, 0.0F);
        player->condition = EntCondition::Normal;
        player->stun_timer = 0;
        player->grounded = true;
        player->holding = false;
        player->holding_vid.reset();
        player->held_by_vid.reset();
        player->attach_mode = AttachMode::None;
        player->facing = Side::Left;
        state.UpdateSidForEnt(player->vid.id, graphics);
    }
    p2->facing = Side::Left;
    return true;
}

bool ValidateActiveSmokePlayers(
    const State& state,
    const char* context,
    std::ostream& error_stream
) {
    std::size_t active_player_ents = 0;
    for (const Ent& ent : state.ents.ents) {
        if (ent.active && ent.type_ == EntType::Player) {
            active_player_ents += 1;
        }
    }
    if (active_player_ents != 2) {
        error_stream << context << " failed: expected 2 active player ents, found "
                     << active_player_ents << '\n';
        return false;
    }

    for (PlayerId player_id : {PlayerId{1}, PlayerId{2}}) {
        const PlayerSlot* const slot = state.players.Find(player_id);
        if (slot == nullptr || !slot->connected || !slot->ent_vid.has_value()) {
            error_stream << context << " failed: missing player slot " << player_id << '\n';
            return false;
        }
        const Ent* const player = state.ents.GetEnt(*slot->ent_vid);
        if (player == nullptr || !player->active || player->type_ != EntType::Player) {
            error_stream << context << " failed: inactive player ent for slot "
                         << player_id << '\n';
            return false;
        }
    }
    return true;
}

bool ValidateNoPlayerCarryLinks(
    const State& state,
    const char* context,
    std::ostream& error_stream
) {
    for (const Ent& ent : state.ents.ents) {
        if (!ent.active || ent.type_ != EntType::Player) {
            continue;
        }
        if (ent.holding_vid.has_value()) {
            const Ent* const held = state.ents.GetEnt(*ent.holding_vid);
            if (held != nullptr && held->type_ == EntType::Player) {
                error_stream << context << " failed: player " << ent.vid.id
                             << " still holds player " << held->vid.id << '\n';
                return false;
            }
        }
        if (ent.held_by_vid.has_value()) {
            const Ent* const holder = state.ents.GetEnt(*ent.held_by_vid);
            if (holder != nullptr && holder->type_ == EntType::Player) {
                error_stream << context << " failed: player " << ent.vid.id
                             << " still held by player " << holder->vid.id << '\n';
                return false;
            }
        }
    }
    return true;
}

struct FakeLockstepInFlightPacket {
    network::LockstepPeerId from_peer = 0;
    network::LockstepPeerId to_peer = 0;
    network::LockstepInputPacket packet;
    std::uint64_t due_tick = 0;
    std::uint64_t insertion_order = 0;
};

struct FakeLockstepNetwork {
    network::NetFuzzerConfig fuzzer;
    network::NetFuzzerStats stats;
    DetRng rng = DetRng::New(1U);
    std::vector<FakeLockstepInFlightPacket> in_flight;
    std::uint64_t next_insertion_order = 0;

    static FakeLockstepNetwork New(const network::NetFuzzerConfig& config, std::uint32_t seed) {
        FakeLockstepNetwork network;
        network.fuzzer = config;
        network.rng = DetRng::New(seed);
        return network;
    }

    bool RollPercent(float percent) {
        return percent > 0.0F && rng.RandomFloat(0.0F, 100.0F) < percent;
    }

    std::uint64_t ComputeDelayTicks() {
        if (!fuzzer.enabled) {
            return 0;
        }

        float delay_ms = fuzzer.latency_ms;
        if (fuzzer.jitter_ms > 0.0F) {
            delay_ms += rng.RandomFloat(-fuzzer.jitter_ms, fuzzer.jitter_ms);
        }
        delay_ms = std::max(0.0F, delay_ms);

        constexpr float kTickMs = 1000.0F / static_cast<float>(kFramesPerSecond);
        std::uint64_t delay_ticks =
            static_cast<std::uint64_t>(CeilToInt(delay_ms / kTickMs));
        if (fuzzer.reorder_window_packets > 0) {
            const int reorder_window = static_cast<int>(fuzzer.reorder_window_packets);
            delay_ticks += static_cast<std::uint64_t>(rng.RandomIntInclusive(0, reorder_window));
        }
        return delay_ticks;
    }

    void QueueOne(
        std::uint64_t now_tick,
        network::LockstepPeerId from_peer,
        network::LockstepPeerId to_peer,
        const network::LockstepInputPacket& packet
    ) {
        FakeLockstepInFlightPacket in_flight_packet;
        in_flight_packet.from_peer = from_peer;
        in_flight_packet.to_peer = to_peer;
        in_flight_packet.packet = packet;
        in_flight_packet.due_tick = now_tick + ComputeDelayTicks();
        in_flight_packet.insertion_order = next_insertion_order++;
        in_flight.push_back(in_flight_packet);
    }

    void Send(
        std::uint64_t now_tick,
        network::LockstepPeerId from_peer,
        network::LockstepPeerId to_peer,
        const network::LockstepInputPacket& packet
    ) {
        stats.packets_sent += 1;
        if (RollPercent(fuzzer.packet_loss_percent)) {
            stats.packets_dropped += 1;
            return;
        }

        QueueOne(now_tick, from_peer, to_peer, packet);
        if (RollPercent(fuzzer.duplicate_percent)) {
            stats.packets_duplicated += 1;
            QueueOne(now_tick, from_peer, to_peer, packet);
        }
    }

    std::vector<network::LockstepInputPacket> ReceiveForPeer(
        std::uint64_t now_tick,
        network::LockstepPeerId peer_id
    ) {
        std::vector<FakeLockstepInFlightPacket> delivered;
        for (const FakeLockstepInFlightPacket& packet : in_flight) {
            if (packet.to_peer == peer_id && packet.due_tick <= now_tick) {
                delivered.push_back(packet);
            }
        }
        in_flight.erase(
            std::remove_if(
                in_flight.begin(),
                in_flight.end(),
                [&](const FakeLockstepInFlightPacket& packet) {
                    return packet.to_peer == peer_id && packet.due_tick <= now_tick;
                }
            ),
            in_flight.end()
        );

        std::sort(
            delivered.begin(),
            delivered.end(),
            [](const FakeLockstepInFlightPacket& lhs, const FakeLockstepInFlightPacket& rhs) {
                if (lhs.due_tick != rhs.due_tick) {
                    return lhs.due_tick < rhs.due_tick;
                }
                return lhs.insertion_order < rhs.insertion_order;
            }
        );

        std::vector<network::LockstepInputPacket> packets;
        packets.reserve(delivered.size());
        for (const FakeLockstepInFlightPacket& packet : delivered) {
            stats.packets_received += 1;
            packets.push_back(packet.packet);
        }
        return packets;
    }
};

struct FakeLockstepPeer {
    network::LockstepPeerId peer_id = 0;
    std::vector<PlayerId> owned_players;
    State state = State::New();
    network::LockstepInputBuffer input_buffer;
    network::LockstepFrame next_frame_to_step = 0;
    std::uint32_t next_packet_sequence = 1;
    std::vector<std::uint64_t> frame_hashes;
};

struct FakeLockstepRunRateSchedule {
    std::uint32_t peer0_pump_every_ticks = 1;
    std::uint32_t peer1_pump_every_ticks = 1;
    std::uint32_t peer0_hitch_every_ticks = 0;
    std::uint32_t peer0_hitch_length_ticks = 0;
    std::uint32_t peer1_hitch_every_ticks = 0;
    std::uint32_t peer1_hitch_length_ticks = 0;
};

bool ShouldPumpFakePeer(
    const FakeLockstepRunRateSchedule& schedule,
    network::LockstepPeerId peer_id,
    std::uint64_t wall_tick
) {
    const std::uint32_t pump_every = peer_id == 0
        ? schedule.peer0_pump_every_ticks
        : schedule.peer1_pump_every_ticks;
    if (pump_every > 1 && wall_tick % pump_every != 0) {
        return false;
    }

    const std::uint32_t hitch_every = peer_id == 0
        ? schedule.peer0_hitch_every_ticks
        : schedule.peer1_hitch_every_ticks;
    const std::uint32_t hitch_length = peer_id == 0
        ? schedule.peer0_hitch_length_ticks
        : schedule.peer1_hitch_length_ticks;
    if (hitch_every > 0 && hitch_length > 0 &&
        wall_tick % hitch_every < hitch_length) {
        return false;
    }
    return true;
}

InputFrame GetLockstepScriptInput(
    const std::vector<std::array<InputFrame, 2>>& script,
    PlayerId player_id,
    network::LockstepFrame frame
) {
    if (frame >= script.size()) {
        return InputFrame::New();
    }
    const std::size_t frame_index = static_cast<std::size_t>(frame);
    if (player_id == 1) {
        return script[frame_index][0];
    }
    if (player_id == 2) {
        return script[frame_index][1];
    }
    return InputFrame::New();
}

network::LockstepInputPacket BuildLockstepInputPacket(
    FakeLockstepPeer& peer,
    const std::vector<std::array<InputFrame, 2>>& script,
    network::LockstepFrame latest_frame
) {
    network::LockstepInputPacket packet;
    packet.session_id = 0x51A7E001U;
    packet.stage_instance_id = 1U;
    packet.sender_peer_id = peer.peer_id;
    packet.sequence = peer.next_packet_sequence++;

    for (network::LockstepFrame frame = 0; frame <= latest_frame; ++frame) {
        for (PlayerId player_id : peer.owned_players) {
            network::LockstepInputRecord record;
            record.player_id = player_id;
            record.frame = frame;
            record.sequence = packet.sequence;
            record.input = GetLockstepScriptInput(script, player_id, frame);
            peer.input_buffer.Store(record);
            packet.records.push_back(record);
        }
    }
    return packet;
}

bool ConfigureLockstepSmokeOwnership(State& state, const std::vector<PlayerId>& owned_players) {
    for (PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.player_id == kInvalidPlayerId) {
            continue;
        }
        const bool owned =
            std::find(owned_players.begin(), owned_players.end(), slot.player_id) != owned_players.end();
        slot.connection_kind = owned ? PlayerConnectionKind::Local : PlayerConnectionKind::Remote;
        slot.primary_local = false;
    }

    if (!owned_players.empty()) {
        if (PlayerSlot* const primary = state.players.Find(owned_players.front())) {
            primary->primary_local = true;
        }
    }
    return state.players.FindPrimaryLocal() != nullptr;
}

bool PrepareLockstepSmokeState(
    State& state,
    Graphics& graphics,
    const std::vector<PlayerId>& owned_players,
    const char*& failed_step
) {
    constexpr std::uint32_t seed = 12345;
    state.net_session.input_lockstep_enabled = true;
    if (!LoadQuestStage(state, "classic", "classic_mines_1", false, seed)) {
        failed_step = "load stage";
        return false;
    }
    if (!PrepareBroadDetReplayScenario(state, failed_step)) {
        return false;
    }
    if (!AddSecondLocalPlayerForDetReplay(state, graphics)) {
        failed_step = "spawn second player";
        return false;
    }
    if (!ConfigureLockstepSmokeOwnership(state, owned_players)) {
        failed_step = "configure lockstep ownership";
        return false;
    }
    state.SetMode(Mode::Playing);
    return true;
}

bool PrepareRespawnPolicySmokeEntrance(State& state, const char*& failed_step) {
    if (!SetScenarioForegroundTile(state, IVec2::New(4, 19), Tile::Entrance)) {
        failed_step = "prepare respawn entrance";
        return false;
    }
    return true;
}

bool KillSmokePlayer(State& state, PlayerId player_id, const char*& failed_step) {
    const PlayerSlot* const slot = state.players.Find(player_id);
    if (slot == nullptr || !slot->ent_vid.has_value()) {
        failed_step = "find smoke player to kill";
        return false;
    }
    Ent* player = state.ents.GetEntMut(*slot->ent_vid);
    if (player == nullptr || !player->active) {
        failed_step = "resolve smoke player to kill";
        return false;
    }
    player->condition = EntCondition::Dead;
    player->health = 0;
    return true;
}

bool SmokePlayerIsAlive(const State& state, PlayerId player_id) {
    const PlayerSlot* const slot = state.players.Find(player_id);
    if (slot == nullptr || !slot->ent_vid.has_value()) {
        return false;
    }
    const Ent* const player = state.ents.GetEnt(*slot->ent_vid);
    return player != nullptr && player->active && player->condition != EntCondition::Dead;
}

bool SmokePlayerHasNoActiveBody(const State& state, PlayerId player_id) {
    const PlayerSlot* const slot = state.players.Find(player_id);
    if (slot == nullptr) {
        return false;
    }
    if (!slot->ent_vid.has_value()) {
        return true;
    }
    const Ent* const player = state.ents.GetEnt(*slot->ent_vid);
    return player == nullptr || !player->active || player->condition == EntCondition::Dead;
}

bool RemoveSmokePlayerBody(State& state, PlayerId player_id, const char*& failed_step) {
    PlayerSlot* const slot = state.players.Find(player_id);
    if (slot == nullptr || !slot->ent_vid.has_value()) {
        failed_step = "find smoke player body to remove";
        return false;
    }
    state.ents.SetInactiveVid(*slot->ent_vid);
    slot->ent_vid.reset();
    return true;
}

bool SmokePlayerOwnershipMatches(
    const State& state,
    PlayerId player_id,
    PlayerConnectionKind expected_kind,
    bool expected_primary
) {
    const PlayerSlot* const slot = state.players.Find(player_id);
    return slot != nullptr &&
           slot->connected &&
           slot->connection_kind == expected_kind &&
           slot->primary_local == expected_primary;
}

std::string DescribeSmokePlayerOwnership(const State& state) {
    std::ostringstream out;
    for (const PlayerSlot& slot : state.players.slots) {
        out << " p" << slot.player_id
            << ":" << (slot.connection_kind == PlayerConnectionKind::Local ? "local" : "remote")
            << ":primary=" << (slot.primary_local ? "true" : "false")
            << ":connected=" << (slot.connected ? "true" : "false");
    }
    return out.str();
}

void ConfigureSmokeNetworkRoles(State& peer0, State& peer1) {
    peer0.net_session.role = network::NetRole::Host;
    peer0.net_session.host_player_id = 1;
    peer0.net_session.local_player_id = 1;
    peer1.net_session.role = network::NetRole::Peer;
    peer1.net_session.host_player_id = 1;
    peer1.net_session.local_player_id = 2;
}

bool RunSmokeStageTransition(
    State& peer0,
    State& peer1,
    Audio& peer0_audio,
    Audio& peer1_audio,
    Graphics& peer0_graphics,
    Graphics& peer1_graphics,
    std::uint32_t seed
) {
    const StageTransitionTarget transition{
        .destination = StageLoadTarget::ForQuestStage("classic", "classic_mines_2"),
        .preserve_player_state = true,
        .seed = seed,
    };
    QueueStageTransition(peer0, transition);
    QueueStageTransition(peer1, transition);
    peer0.SetMode(Mode::StageTransition);
    peer1.SetMode(Mode::StageTransition);
    peer0.scene_frame = 0;
    peer1.scene_frame = 0;
    for (std::uint32_t i = 0; i < 70; ++i) {
        StepSingleTick(peer0, peer0_audio, peer0_graphics);
        StepSingleTick(peer1, peer1_audio, peer1_graphics);
    }
    return peer0.stage.quest_stage_id == "classic_mines_2" &&
           peer1.stage.quest_stage_id == "classic_mines_2";
}

bool RunNetworkFreshQuestReloadSmoke(
    State& peer0,
    State& peer1,
    Audio& peer0_audio,
    Audio& peer1_audio,
    Graphics& peer0_graphics,
    Graphics& peer1_graphics,
    std::uint32_t seed
) {
    const StageTransitionTarget transition{
        .destination = StageLoadTarget::ForQuestStage("classic", "classic_mines_1"),
        .preserve_player_state = false,
        .seed = seed,
    };
    QueueStageTransition(peer0, transition);
    QueueStageTransition(peer1, transition);
    peer0.SetMode(Mode::StageTransition);
    peer1.SetMode(Mode::StageTransition);
    peer0.scene_frame = 0;
    peer1.scene_frame = 0;
    for (std::uint32_t i = 0; i < 70; ++i) {
        StepSingleTick(peer0, peer0_audio, peer0_graphics);
        StepSingleTick(peer1, peer1_audio, peer1_graphics);
    }
    return peer0.stage.quest_stage_id == "classic_mines_1" &&
           peer1.stage.quest_stage_id == "classic_mines_1";
}

void ApplyLockstepInputsToState(
    State& state,
    const std::vector<PlayerId>& player_ids,
    const std::vector<InputFrame>& input_frames
) {
    const PlayerSlot* const primary_slot = state.players.FindPrimaryLocal();
    const PlayerId primary_player_id =
        primary_slot != nullptr ? primary_slot->player_id : kInvalidPlayerId;

    for (std::size_t i = 0; i < player_ids.size(); ++i) {
        if (player_ids[i] == primary_player_id) {
            ApplyPrimaryInputFrame(state, input_frames[i]);
        } else {
            state.players.SetInputFrameForPlayer(player_ids[i], input_frames[i]);
        }
    }
}

bool StepReadyLockstepFrames(
    FakeLockstepPeer& peer,
    const std::vector<PlayerId>& required_players,
    Audio& audio,
    Graphics& graphics,
    network::LockstepFrame total_frames,
    std::string& error
) {
    std::vector<InputFrame> frame_inputs;
    while (peer.next_frame_to_step < total_frames &&
           peer.input_buffer.BuildFrameInputs(
               required_players,
               peer.next_frame_to_step,
               frame_inputs
           )) {
        ApplyLockstepInputsToState(peer.state, required_players, frame_inputs);
        StepSingleTickWithMode(
            peer.state,
            audio,
            graphics,
            SimulationTickMode::ReplayNoNetwork
        );
        const CanonicalStateFingerprint fingerprint =
            ComputeNetworkStateFingerprint(peer.state);
        peer.frame_hashes.push_back(fingerprint.value);
        peer.next_frame_to_step += 1;

        if (peer.next_frame_to_step > 4) {
            peer.input_buffer.ClearBefore(peer.next_frame_to_step - 4);
        }
    }

    if (peer.next_frame_to_step > peer.frame_hashes.size()) {
        error = "lockstep peer advanced without recording hash";
        return false;
    }
    return true;
}

bool RunCanonicalInputBufferSmoke() {
    network::LockstepInputBuffer buffer;

    InputFrame predicted_input = InputFrame::New();
    predicted_input.right = true;
    network::LockstepInputRecord predicted;
    predicted.player_id = 2;
    predicted.frame = 5;
    predicted.sequence = 1;
    predicted.input = predicted_input;
    predicted.predicted = true;
    predicted.canonical = false;
    if (!buffer.Store(predicted).inserted) {
        std::cerr << "canonical input buffer smoke failed: predicted insert failed\n";
        return false;
    }

    network::LockstepInputRecord canonical_match = predicted;
    canonical_match.sequence = 2;
    canonical_match.predicted = false;
    canonical_match.canonical = true;
    const network::LockstepInputStoreResult match_result = buffer.Store(canonical_match);
    if (!match_result.replaced_prediction || match_result.mismatch_frame.has_value()) {
        std::cerr << "canonical input buffer smoke failed: matching canonical replacement not tracked\n";
        return false;
    }
    const network::LockstepInputRecord* const matched_record = buffer.FindRecord(2, 5);
    if (matched_record == nullptr || matched_record->predicted || !matched_record->canonical) {
        std::cerr << "canonical input buffer smoke failed: matching canonical record not stored\n";
        return false;
    }

    network::LockstepInputRecord predicted_wrong = predicted;
    predicted_wrong.frame = 6;
    predicted_wrong.sequence = 3;
    predicted_wrong.input.left = false;
    predicted_wrong.input.right = true;
    if (!buffer.Store(predicted_wrong).inserted) {
        std::cerr << "canonical input buffer smoke failed: second predicted insert failed\n";
        return false;
    }

    network::LockstepInputRecord canonical_mismatch = predicted_wrong;
    canonical_mismatch.sequence = 4;
    canonical_mismatch.input.left = true;
    canonical_mismatch.input.right = false;
    canonical_mismatch.predicted = false;
    canonical_mismatch.canonical = true;
    const network::LockstepInputStoreResult mismatch_result =
        buffer.Store(canonical_mismatch);
    if (!mismatch_result.replaced_prediction ||
        !mismatch_result.mismatch_frame.has_value() ||
        *mismatch_result.mismatch_frame != 6) {
        std::cerr << "canonical input buffer smoke failed: mismatching canonical replacement not tracked\n";
        return false;
    }

    network::LockstepInputRecord stale_noncanonical = canonical_mismatch;
    stale_noncanonical.sequence = 5;
    stale_noncanonical.input.left = false;
    stale_noncanonical.input.right = true;
    stale_noncanonical.canonical = false;
    (void)buffer.Store(stale_noncanonical);
    const network::LockstepInputRecord* const canonical_record = buffer.FindRecord(2, 6);
    if (canonical_record == nullptr || !canonical_record->canonical ||
        canonical_record->input.right || !canonical_record->input.left) {
        std::cerr << "canonical input buffer smoke failed: noncanonical input overwrote canonical record\n";
        return false;
    }

    network::LockstepInputRecord arbitrated_guess = canonical_mismatch;
    arbitrated_guess.frame = 7;
    arbitrated_guess.sequence = 6;
    arbitrated_guess.input.left = false;
    arbitrated_guess.input.right = true;
    arbitrated_guess.canonical = true;
    arbitrated_guess.arbitrated_missing = true;
    if (!buffer.Store(arbitrated_guess).inserted) {
        std::cerr << "canonical input buffer smoke failed: arbitrated insert failed\n";
        return false;
    }

    network::LockstepInputRecord late_owner_input = arbitrated_guess;
    late_owner_input.sequence = 7;
    late_owner_input.input.left = true;
    late_owner_input.input.right = false;
    late_owner_input.canonical = false;
    late_owner_input.arbitrated_missing = false;
    const network::LockstepInputStoreResult late_result = buffer.Store(late_owner_input);
    if (!late_result.changed_existing ||
        !late_result.mismatch_frame.has_value() ||
        *late_result.mismatch_frame != 7) {
        std::cerr << "canonical input buffer smoke failed: late owner input did not replace arbitrated guess\n";
        return false;
    }
    const network::LockstepInputRecord* const late_record = buffer.FindRecord(2, 7);
    if (late_record == nullptr || late_record->canonical ||
        !late_record->input.left || late_record->input.right) {
        std::cerr << "canonical input buffer smoke failed: late owner input not retained for rollback\n";
        return false;
    }
    if (!buffer.HasNonCanonicalRecordThroughFrame({2}, 7)) {
        std::cerr << "canonical input buffer smoke failed: noncanonical history not detected\n";
        return false;
    }
    if (buffer.HasNonCanonicalRecordThroughFrame({2}, 6)) {
        std::cerr << "canonical input buffer smoke failed: noncanonical history leaked backward\n";
        return false;
    }

    std::cout << "canonical input buffer smoke ok\n";
    return true;
}

bool RunRollbackRepairSmoke() {
    Graphics truth_graphics;
    Graphics predicted_graphics;
    InitCliSmokeRuntimeTables(truth_graphics);
    InitCliSmokeRuntimeTables(predicted_graphics);
    Audio truth_audio;
    Audio predicted_audio;

    State truth = State::New();
    State predicted = State::New();
    const std::vector<PlayerId> players = {1, 2};
    const char* failed_step = nullptr;
    if (!PrepareLockstepSmokeState(truth, truth_graphics, players, failed_step) ||
        !PrepareLockstepSmokeState(predicted, predicted_graphics, players, failed_step)) {
        std::cerr << "rollback repair smoke failed during setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }

    InputFrame neutral = InputFrame::New();
    InputFrame remote_actual = InputFrame::New();
    remote_actual.right = true;
    remote_actual.run = true;

    const SimSnapshot pre_frame_0 = MakeSimSnapshot(predicted);

    ApplyLockstepInputsToState(truth, players, {neutral, remote_actual});
    StepSingleTickWithMode(truth, truth_audio, truth_graphics, SimulationTickMode::ReplayNoNetwork);
    ApplyLockstepInputsToState(predicted, players, {neutral, neutral});
    StepSingleTickWithMode(
        predicted,
        predicted_audio,
        predicted_graphics,
        SimulationTickMode::ReplayNoNetwork
    );

    ApplyLockstepInputsToState(truth, players, {neutral, remote_actual});
    StepSingleTickWithMode(truth, truth_audio, truth_graphics, SimulationTickMode::ReplayNoNetwork);
    ApplyLockstepInputsToState(predicted, players, {neutral, neutral});
    StepSingleTickWithMode(
        predicted,
        predicted_audio,
        predicted_graphics,
        SimulationTickMode::ReplayNoNetwork
    );
    if (ComputeGameplayDeterminismFingerprint(truth).value ==
        ComputeGameplayDeterminismFingerprint(predicted).value) {
        std::cerr << "rollback repair smoke failed: prediction did not diverge\n";
        return false;
    }

    network::LockstepInputBuffer buffer;
    network::LockstepInputRecord predicted_record;
    predicted_record.player_id = 2;
    predicted_record.frame = 0;
    predicted_record.input = neutral;
    predicted_record.predicted = true;
    (void)buffer.Store(predicted_record);

    network::LockstepInputRecord actual_record = predicted_record;
    actual_record.input = remote_actual;
    actual_record.predicted = false;
    const network::LockstepInputStoreResult store_result = buffer.Store(actual_record);
    if (!store_result.mismatch_frame.has_value() || *store_result.mismatch_frame != 0) {
        std::cerr << "rollback repair smoke failed: predicted input mismatch not detected\n";
        return false;
    }

    RestoreSimSnapshot(pre_frame_0, predicted, predicted_graphics);
    ApplyLockstepInputsToState(predicted, players, {neutral, remote_actual});
    StepSingleTickWithMode(
        predicted,
        predicted_audio,
        predicted_graphics,
        SimulationTickMode::ReplayNoNetwork
    );
    ApplyLockstepInputsToState(predicted, players, {neutral, remote_actual});
    StepSingleTickWithMode(
        predicted,
        predicted_audio,
        predicted_graphics,
        SimulationTickMode::ReplayNoNetwork
    );

    if (!CompareCanonicalFingerprints(truth, predicted, "rollback repair final")) {
        std::cerr << "  first simple diff: "
                  << DescribeFirstStateDifference(truth, predicted) << '\n';
        return false;
    }

    std::cout << "rollback repair smoke ok\n";
    return true;
}

bool RunJoinBarrierProtocolSmoke() {
    network::JoinRequestPacket join_request;
    join_request.local_player_count = 3;
    join_request.preferred_player_count = 2;
    join_request.preferred_player_ids[0] = 7;
    join_request.preferred_player_ids[1] = 8;
    join_request.content_hash = 0x123456789ABCDEF0ULL;
    const network::EncodedNetPacket encoded_join_request =
        network::EncodeJoinRequest(join_request);
    const std::optional<network::JoinRequestPacket> decoded_join_request =
        network::TryDecodeJoinRequest(
            encoded_join_request.bytes.data(),
            encoded_join_request.size
        );
    if (!decoded_join_request.has_value() ||
        decoded_join_request->local_player_count != join_request.local_player_count ||
        decoded_join_request->preferred_player_count != join_request.preferred_player_count ||
        decoded_join_request->preferred_player_ids[0] != join_request.preferred_player_ids[0] ||
        decoded_join_request->preferred_player_ids[1] != join_request.preferred_player_ids[1] ||
        decoded_join_request->content_hash != join_request.content_hash) {
        std::cerr << "join barrier protocol smoke failed: join request packet roundtrip mismatch\n";
        return false;
    }

    network::JoinAcceptPacket join_accept;
    join_accept.assigned_player_count = 2;
    join_accept.assigned_player_ids[0] = 7;
    join_accept.assigned_player_ids[1] = 8;
    join_accept.host_player_id = 1;
    join_accept.stage_instance_id = 44;
    join_accept.remote_spawn_x = sim::ToSimScalar(123.25F);
    join_accept.remote_spawn_y = sim::ToSimScalar(45.5F);
    join_accept.host_spawn_x = sim::ToSimScalar(32.75F);
    join_accept.host_spawn_y = sim::ToSimScalar(64.125F);
    join_accept.stage_seed = 9876U;
    join_accept.lockstep_start_frame = 120;
    join_accept.content_hash = join_request.content_hash;
    const network::EncodedNetPacket encoded_join_accept =
        network::EncodeJoinAccept(join_accept);
    const std::optional<network::JoinAcceptPacket> decoded_join_accept =
        network::TryDecodeJoinAccept(
            encoded_join_accept.bytes.data(),
            encoded_join_accept.size
        );
    if (!decoded_join_accept.has_value() ||
        decoded_join_accept->assigned_player_count != join_accept.assigned_player_count ||
        decoded_join_accept->assigned_player_ids[0] != join_accept.assigned_player_ids[0] ||
        decoded_join_accept->assigned_player_ids[1] != join_accept.assigned_player_ids[1] ||
        decoded_join_accept->host_player_id != join_accept.host_player_id ||
        decoded_join_accept->stage_instance_id != join_accept.stage_instance_id ||
        decoded_join_accept->remote_spawn_x != join_accept.remote_spawn_x ||
        decoded_join_accept->remote_spawn_y != join_accept.remote_spawn_y ||
        decoded_join_accept->host_spawn_x != join_accept.host_spawn_x ||
        decoded_join_accept->host_spawn_y != join_accept.host_spawn_y ||
        decoded_join_accept->stage_seed != join_accept.stage_seed ||
        decoded_join_accept->lockstep_start_frame != join_accept.lockstep_start_frame ||
        decoded_join_accept->content_hash != join_accept.content_hash) {
        std::cerr << "join barrier protocol smoke failed: join accept packet roundtrip mismatch\n";
        return false;
    }

    State host = State::New();
    host.net_session.role = network::NetRole::Host;
    host.net_session.local_player_id = 1;
    host.net_session.stage_instance_id = 44;
    host.net_session.lockstep_next_frame_to_step = 120;

    network::BeginJoinBarrierCatchup(host, 4);
    if (!host.net_session.join_barrier_active ||
        host.net_session.join_barrier_id != 1 ||
        host.net_session.join_barrier_phase != network::JoinBarrierPhase::WaitingForCatchup ||
        host.net_session.join_barrier_queue.size() != 1 ||
        host.net_session.join_barrier_queue[0] != 4) {
        std::cerr << "join barrier protocol smoke failed: first late join was not queued\n";
        return false;
    }

    host.net_session.join_barrier_active_peer_id = 4;
    host.net_session.join_barrier_phase = network::JoinBarrierPhase::SendingSnapshot;
    host.net_session.join_barrier_queue.clear();
    host.net_session.join_barrier_transfer_id = 77;
    host.net_session.join_barrier_snapshot_frame = 120;
    host.net_session.join_barrier_chunk_count = 10;
    host.net_session.join_barrier_chunks_done = 4;
    host.net_session.join_barrier_total_bytes = 4096;
    host.net_session.join_barrier_bytes_done = 1600;

    network::BeginJoinBarrierCatchup(host, 4);
    network::BeginJoinBarrierCatchup(host, 5);
    if (host.net_session.join_barrier_queue.size() != 2 ||
        host.net_session.join_barrier_queue[0] != 4 ||
        host.net_session.join_barrier_queue[1] != 5 ||
        host.net_session.join_barrier_active_peer_id != 4) {
        std::cerr << "join barrier protocol smoke failed: active catchup peer was not requeued after topology change\n";
        return false;
    }

    State simultaneous_host = State::New();
    simultaneous_host.net_session.role = network::NetRole::Host;
    simultaneous_host.net_session.local_player_id = 1;
    simultaneous_host.net_session.stage_instance_id = host.net_session.stage_instance_id;
    simultaneous_host.net_session.lockstep_next_frame_to_step = 160;
    network::NetTransportRuntime simultaneous_transport = network::NetTransportRuntime::New();
    simultaneous_transport.remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39202},
        .last_heard_frame = 0,
        .last_heard_pump_tick = 100,
    });
    network::BeginJoinBarrierTopologyChange(simultaneous_host, simultaneous_transport, {2});
    if (!simultaneous_host.net_session.join_barrier_active ||
        simultaneous_host.net_session.join_barrier_queue.size() != 1 ||
        simultaneous_host.net_session.join_barrier_queue[0] != 2 ||
        simultaneous_host.net_session.join_barrier_joined_player_ids.size() != 1 ||
        simultaneous_host.net_session.join_barrier_joined_player_ids[0] != 2) {
        std::cerr << "join barrier protocol smoke failed: first topology join was not queued for snapshot\n";
        return false;
    }

    simultaneous_host.net_session.join_barrier_active_peer_id = 2;
    simultaneous_host.net_session.join_barrier_phase =
        network::JoinBarrierPhase::SendingSnapshot;
    simultaneous_host.net_session.join_barrier_queue.clear();
    simultaneous_host.net_session.join_barrier_transfer_id = 88;
    simultaneous_transport.remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {3},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39203},
        .last_heard_frame = 0,
        .last_heard_pump_tick = 100,
    });
    network::BeginJoinBarrierTopologyChange(simultaneous_host, simultaneous_transport, {3});
    if (simultaneous_host.net_session.join_barrier_queue.size() != 1 ||
        simultaneous_host.net_session.join_barrier_queue[0] != 3 ||
        simultaneous_host.net_session.join_barrier_joined_player_ids.size() != 2 ||
        simultaneous_host.net_session.join_barrier_joined_player_ids[1] != 3 ||
        simultaneous_host.net_session.join_barrier_topology_ack_peers.size() != 1 ||
        simultaneous_host.net_session.join_barrier_topology_ack_peers[0] != 2) {
        std::cerr << "join barrier protocol smoke failed: simultaneous late join did not queue new peer and defer active peer topology\n";
        return false;
    }

    State multi_local_host = State::New();
    multi_local_host.net_session.role = network::NetRole::Host;
    multi_local_host.net_session.local_player_id = 1;
    multi_local_host.net_session.stage_instance_id = host.net_session.stage_instance_id;
    network::NetTransportRuntime multi_local_transport = network::NetTransportRuntime::New();
    multi_local_transport.remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {3, 4, 5},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39205},
        .last_heard_frame = 0,
        .last_heard_pump_tick = 100,
    });
    network::BeginJoinBarrierTopologyChange(multi_local_host, multi_local_transport, {3, 4, 5});
    if (multi_local_host.net_session.join_barrier_queue.size() != 1 ||
        multi_local_host.net_session.join_barrier_queue[0] != 3 ||
        multi_local_host.net_session.join_barrier_joined_player_ids.size() != 3) {
        std::cerr << "join barrier protocol smoke failed: multi-local endpoint queued more than one catchup target\n";
        return false;
    }

    network::JoinBarrierStatusPacket status;
    status.stage_instance_id = host.net_session.stage_instance_id;
    status.sender_peer_id = static_cast<std::uint32_t>(host.net_session.local_player_id);
    status.barrier_id = host.net_session.join_barrier_id;
    status.active = 1;
    status.phase = static_cast<std::uint8_t>(host.net_session.join_barrier_phase);
    status.active_player_id = host.net_session.join_barrier_active_peer_id;
    status.queued_peer_count = 2;
    status.queued_peer_ids[0] = host.net_session.join_barrier_queue[0];
    status.queued_peer_ids[1] = host.net_session.join_barrier_queue[1];
    status.transfer_id = host.net_session.join_barrier_transfer_id;
    status.snapshot_frame = host.net_session.join_barrier_snapshot_frame;
    status.chunk_count = host.net_session.join_barrier_chunk_count;
    status.chunks_done = host.net_session.join_barrier_chunks_done;
    status.total_bytes = host.net_session.join_barrier_total_bytes;
    status.bytes_done = host.net_session.join_barrier_bytes_done;

    const network::EncodedNetPacket encoded_status = network::EncodeJoinBarrierStatus(status);
    const std::optional<network::JoinBarrierStatusPacket> decoded_status =
        network::TryDecodeJoinBarrierStatus(encoded_status.bytes.data(), encoded_status.size);
    if (!decoded_status.has_value() ||
        decoded_status->stage_instance_id != status.stage_instance_id ||
        decoded_status->barrier_id != status.barrier_id ||
        decoded_status->active != status.active ||
        decoded_status->phase != status.phase ||
        decoded_status->active_player_id != status.active_player_id ||
        decoded_status->queued_peer_count != status.queued_peer_count ||
        decoded_status->queued_peer_ids[0] != status.queued_peer_ids[0] ||
        decoded_status->queued_peer_ids[1] != status.queued_peer_ids[1] ||
        decoded_status->transfer_id != status.transfer_id ||
        decoded_status->snapshot_frame != status.snapshot_frame ||
        decoded_status->chunk_count != status.chunk_count ||
        decoded_status->chunks_done != status.chunks_done ||
        decoded_status->total_bytes != status.total_bytes ||
        decoded_status->bytes_done != status.bytes_done) {
        std::cerr << "join barrier protocol smoke failed: status packet roundtrip mismatch\n";
        return false;
    }

    network::JoinBarrierTopologyPacket topology;
    topology.stage_instance_id = host.net_session.stage_instance_id;
    topology.sender_peer_id = static_cast<std::uint32_t>(host.net_session.local_player_id);
    topology.barrier_id = host.net_session.join_barrier_id;
    topology.barrier_frame = host.net_session.lockstep_next_frame_to_step;
    topology.player_count = 1;
    topology.player_ids[0] = 6;
    const sim::Vec2 topology_pos = sim::ToSimVec2(Vec2::New(128.0F, 64.0F));
    topology.player_pos_x_raw[0] = topology_pos.x.raw_value();
    topology.player_pos_y_raw[0] = topology_pos.y.raw_value();
    topology.removed_player_count = 1;
    topology.removed_player_ids[0] = 4;
    const network::EncodedNetPacket encoded_topology =
        network::EncodeJoinBarrierTopology(topology);
    const std::optional<network::JoinBarrierTopologyPacket> decoded_topology =
        network::TryDecodeJoinBarrierTopology(
            encoded_topology.bytes.data(),
            encoded_topology.size
        );
    if (!decoded_topology.has_value() ||
        decoded_topology->stage_instance_id != topology.stage_instance_id ||
        decoded_topology->barrier_id != topology.barrier_id ||
        decoded_topology->barrier_frame != topology.barrier_frame ||
        decoded_topology->player_count != topology.player_count ||
        decoded_topology->player_ids[0] != topology.player_ids[0] ||
        decoded_topology->player_pos_x_raw[0] != topology.player_pos_x_raw[0] ||
        decoded_topology->player_pos_y_raw[0] != topology.player_pos_y_raw[0] ||
        decoded_topology->removed_player_count != topology.removed_player_count ||
        decoded_topology->removed_player_ids[0] != topology.removed_player_ids[0]) {
        std::cerr << "join barrier protocol smoke failed: topology packet roundtrip mismatch\n";
        return false;
    }

    State topology_peer = State::New();
    Graphics topology_graphics;
    InitCliSmokeRuntimeTables(topology_graphics);
    topology_peer.net_session.role = network::NetRole::Peer;
    topology_peer.net_session.local_player_id = 2;
    topology_peer.net_session.stage_instance_id = host.net_session.stage_instance_id;
    topology_peer.players.EnsureLocalPlayer(2, "Player 2", true);
    topology_peer.players.EnsureRemotePlayer(4, "Player 4");
    network::NetTransportRuntime topology_transport = network::NetTransportRuntime::New();
    topology_transport.capture_outgoing_packets = true;
    topology_transport.host_endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39000};
    network::HandleJoinBarrierTopology(
        topology_peer,
        topology_graphics,
        topology_transport,
        *decoded_topology
    );
    const std::optional<network::JoinBarrierTopologyAckPacket> topology_ack =
        topology_transport.captured_packets.empty()
            ? std::nullopt
            : network::TryDecodeJoinBarrierTopologyAck(
                  topology_transport.captured_packets.back().bytes.data(),
                  topology_transport.captured_packets.back().size
              );
    if (topology_peer.players.Find(4) != nullptr ||
        topology_peer.players.Find(6) == nullptr ||
        topology_peer.net_session.join_barrier_phase != network::JoinBarrierPhase::WaitingForResume ||
        !topology_ack.has_value() ||
        topology_ack->barrier_id != topology.barrier_id ||
        topology_ack->sender_peer_id != 2 ||
        topology_ack->success == 0) {
        std::cerr << "join barrier protocol smoke failed: topology add/remove did not apply and ack\n";
        return false;
    }

    State removal_host = State::New();
    removal_host.net_session.role = network::NetRole::Host;
    removal_host.net_session.local_player_id = 1;
    removal_host.net_session.stage_instance_id = host.net_session.stage_instance_id;
    removal_host.net_session.lockstep_next_frame_to_step = 222;
    network::NetTransportRuntime removal_transport = network::NetTransportRuntime::New();
    removal_transport.remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39002},
        .last_heard_frame = 0,
        .last_heard_pump_tick = 100,
    });
    removal_transport.remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {3},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39003},
        .last_heard_frame = 0,
        .last_heard_pump_tick = 100,
    });
    removal_transport.pump_tick = 100;
    network::BeginJoinBarrierTopologyRemoval(removal_host, removal_transport, {4});
    if (!removal_host.net_session.join_barrier_active ||
        removal_host.net_session.join_barrier_removed_player_ids.size() != 1 ||
        removal_host.net_session.join_barrier_removed_player_ids[0] != 4 ||
        removal_host.net_session.join_barrier_topology_ack_peers.size() != 2 ||
        removal_host.net_session.join_barrier_topology_ack_peers[0] != 2 ||
        removal_host.net_session.join_barrier_topology_ack_peers[1] != 3) {
        std::cerr << "join barrier protocol smoke failed: topology removal did not queue remaining peer acks\n";
        return false;
    }

    State timeout_host = State::New();
    timeout_host.net_session.role = network::NetRole::Host;
    timeout_host.net_session.local_player_id = 1;
    timeout_host.net_session.stage_instance_id = host.net_session.stage_instance_id;
    network::NetTransportRuntime timeout_transport = network::NetTransportRuntime::New();
    timeout_transport.pump_tick = 220;
    timeout_transport.remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39102},
        .last_heard_frame = 0,
        .last_heard_pump_tick = 220,
    });
    timeout_transport.remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {4},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39104},
        .last_heard_frame = 0,
        .last_heard_pump_tick = 0,
    });
    network::CleanupTimedOutRemoteEndpoints(timeout_host, timeout_transport);
    if (timeout_transport.remotes.size() != 1 ||
        timeout_transport.remotes[0].player_ids.size() != 1 ||
        timeout_transport.remotes[0].player_ids[0] != 2 ||
        timeout_host.net_session.join_barrier_removed_player_ids.size() != 1 ||
        timeout_host.net_session.join_barrier_removed_player_ids[0] != 4 ||
        timeout_host.net_session.join_barrier_topology_ack_peers.size() != 1 ||
        timeout_host.net_session.join_barrier_topology_ack_peers[0] != 2) {
        std::cerr << "join barrier protocol smoke failed: pump-tick timeout did not remove dead peer\n";
        return false;
    }

    State relay_timeout_host = State::New();
    relay_timeout_host.net_session.role = network::NetRole::Host;
    relay_timeout_host.net_session.local_player_id = 1;
    relay_timeout_host.net_session.stage_instance_id = host.net_session.stage_instance_id;
    network::NetTransportRuntime relay_timeout_transport = network::NetTransportRuntime::New();
    relay_timeout_transport.pump_tick = 220;
    relay_timeout_transport.remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = network::NetEndpoint{.address = "realnet-relay",
                                         .port = 60000,
                                         .kind = network::NetEndpointKind::RealnetRelayVirtual},
        .last_heard_frame = 0,
        .last_heard_pump_tick = 0,
    });
    network::CleanupTimedOutRemoteEndpoints(relay_timeout_host, relay_timeout_transport);
    if (relay_timeout_transport.remotes.size() != 1 ||
        relay_timeout_transport.remotes[0].player_ids.size() != 1 ||
        relay_timeout_transport.remotes[0].player_ids[0] != 2) {
        std::cerr << "join barrier protocol smoke failed: relay peer used direct timeout\n";
        return false;
    }
    relay_timeout_transport.pump_tick = 4000;
    network::CleanupTimedOutRemoteEndpoints(relay_timeout_host, relay_timeout_transport);
    if (!relay_timeout_transport.remotes.empty()) {
        std::cerr << "join barrier protocol smoke failed: relay peer never timed out\n";
        return false;
    }

    State transition_host = State::New();
    Graphics transition_graphics;
    InitCliSmokeRuntimeTables(transition_graphics);
    transition_host.net_session.role = network::NetRole::Host;
    transition_host.net_session.local_player_id = 1;
    transition_host.net_session.stage_instance_id = host.net_session.stage_instance_id;
    transition_host.SetMode(Mode::StageTransition);
    QueueStageTransition(
        transition_host,
        StageTransitionTarget{
            .destination = StageLoadTarget::ForQuestStage("classic", "classic_mines_2"),
            .preserve_player_state = true,
            .seed = 9876U,
        }
    );
    network::NetTransportRuntime transition_transport = network::NetTransportRuntime::New();
    transition_transport.capture_outgoing_packets = true;
    transition_transport.pump_tick = 100;
    network::UdpPacket transition_join_packet;
    transition_join_packet.endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39105};
    network::JoinRequestPacket transition_join_request;
    transition_join_request.local_player_count = 1;
    transition_join_request.content_hash = ComputeGameplayContentHash();
    network::HandleJoinRequestAsHost(
        transition_host,
        transition_graphics,
        transition_transport,
        transition_join_packet,
        transition_join_request
    );
    if (transition_transport.captured_packets.size() != 1 ||
        !transition_transport.remotes.empty() ||
        transition_transport.pending_join_endpoints.size() != 1 ||
        !transition_host.net_session.join_barrier_queue.empty()) {
        std::cerr << "join barrier protocol smoke failed: host did not defer join during stage transition\n";
        return false;
    }
    const std::optional<network::JoinPendingPacket> decoded_pending =
        network::TryDecodeJoinPending(
            transition_transport.captured_packets.back().bytes.data(),
            transition_transport.captured_packets.back().size
        );
    if (!decoded_pending.has_value() ||
        decoded_pending->reason != network::JoinPendingReason::StageTransition) {
        std::cerr << "join barrier protocol smoke failed: deferred join did not send pending status\n";
        return false;
    }
    transition_host.SetMode(Mode::Playing);
    transition_host.pending_stage_transition.reset();
    network::DrainPendingJoinRequestsAsHost(
        transition_host,
        transition_graphics,
        transition_transport
    );
    if (transition_transport.pending_join_endpoints.empty() == false ||
        transition_transport.remotes.size() != 1 ||
        transition_host.net_session.join_barrier_queue.empty()) {
        std::cerr << "join barrier protocol smoke failed: deferred join did not drain after transition\n";
        return false;
    }

    State mismatch_host = State::New();
    Graphics mismatch_graphics;
    if (!LoadQuestStage(
            mismatch_host,
            "classic",
            "classic_mines_1",
            false,
            9876U
        )) {
        std::cerr << "join barrier protocol smoke failed: mismatch host stage load failed\n";
        return false;
    }
    mismatch_host.net_session.role = network::NetRole::Host;
    mismatch_host.net_session.input_lockstep_enabled = true;
    network::NetTransportRuntime mismatch_transport = network::NetTransportRuntime::New();
    mismatch_transport.capture_outgoing_packets = true;
    network::UdpPacket mismatch_join_packet;
    mismatch_join_packet.endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39106};
    network::JoinRequestPacket mismatch_join_request;
    mismatch_join_request.local_player_count = 1;
    mismatch_join_request.content_hash =
        ComputeGameplayContentHash() ^ 0xA5A5A5A5A5A5A5A5ULL;
    network::HandleJoinRequestAsHost(
        mismatch_host,
        mismatch_graphics,
        mismatch_transport,
        mismatch_join_packet,
        mismatch_join_request
    );
    if (mismatch_transport.captured_packets.size() != 1 ||
        !mismatch_transport.remotes.empty() ||
        !mismatch_host.net_session.join_barrier_queue.empty()) {
        std::cerr << "join barrier protocol smoke failed: content mismatch was not rejected before topology change\n";
        return false;
    }
    const std::optional<network::JoinPendingPacket> decoded_mismatch =
        network::TryDecodeJoinPending(
            mismatch_transport.captured_packets.back().bytes.data(),
            mismatch_transport.captured_packets.back().size
        );
    if (!decoded_mismatch.has_value() ||
        decoded_mismatch->reason != network::JoinPendingReason::ContentMismatch) {
        std::cerr << "join barrier protocol smoke failed: content mismatch did not send mismatch status\n";
        return false;
    }

    State menu_host = State::New();
    Graphics menu_graphics;
    Audio menu_audio;
    InitCliSmokeRuntimeTables(menu_graphics);
    menu_host.SetMode(Mode::Settings);
    menu_host.net_session.role = network::NetRole::Host;
    menu_host.net_session.local_player_id = 1;
    menu_host.net_session.host_player_id = 1;
    menu_host.net_session.input_lockstep_enabled = true;
    menu_host.net_session.stage_instance_id = host.net_session.stage_instance_id;
    menu_host.net_session.join_barrier_active = true;
    menu_host.net_session.join_barrier_id = 33;
    menu_host.net_session.join_barrier_phase = network::JoinBarrierPhase::ReadyToResume;
    menu_host.net_session.lockstep_next_frame_to_step = 240;
    menu_host.net_session.lockstep_next_local_input_frame = 240;
    menu_host.net_transport =
        std::make_unique<network::NetTransportRuntime>(network::NetTransportRuntime::New());
    menu_host.net_transport->capture_outgoing_packets = true;
    std::string menu_socket_error;
    if (!menu_host.net_transport->socket.Open(0, &menu_socket_error)) {
        std::cerr << "join barrier protocol smoke failed opening menu transport socket: "
                  << menu_socket_error << '\n';
        return false;
    }
    menu_host.net_transport->remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39222},
        .last_heard_frame = 0,
        .last_heard_pump_tick = 100,
    });
    StepSingleTick(menu_host, menu_audio, menu_graphics);
    bool menu_resume_sent = false;
    for (const network::UdpPacket& packet : menu_host.net_transport->captured_packets) {
        const std::optional<network::JoinBarrierResumePacket> decoded_menu_resume =
            network::TryDecodeJoinBarrierResume(packet.bytes.data(), packet.size);
        if (decoded_menu_resume.has_value() &&
            decoded_menu_resume->barrier_id == 33 &&
            decoded_menu_resume->resume_frame == 240) {
            menu_resume_sent = true;
            break;
        }
    }
    if (!menu_resume_sent || menu_host.net_session.join_barrier_active) {
        std::cerr << "join barrier protocol smoke failed: menu fixed tick did not resume barrier\n";
        return false;
    }

    State menu_snapshot_host = State::New();
    Graphics menu_snapshot_graphics;
    Audio menu_snapshot_audio;
    InitCliSmokeRuntimeTables(menu_snapshot_graphics);
    const char* menu_snapshot_failed_step = nullptr;
    if (!PrepareLockstepSmokeState(
            menu_snapshot_host,
            menu_snapshot_graphics,
            {1},
            menu_snapshot_failed_step
        )) {
        std::cerr << "join barrier protocol smoke failed during menu snapshot setup: "
                  << (menu_snapshot_failed_step != nullptr ? menu_snapshot_failed_step : "unknown")
                  << '\n';
        return false;
    }
    menu_snapshot_host.SetMode(Mode::Settings);
    menu_snapshot_host.net_session.role = network::NetRole::Host;
    menu_snapshot_host.net_session.local_player_id = 1;
    menu_snapshot_host.net_session.host_player_id = 1;
    menu_snapshot_host.net_session.input_lockstep_enabled = true;
    menu_snapshot_host.net_session.stage_instance_id = host.net_session.stage_instance_id;
    menu_snapshot_host.net_session.lockstep_next_frame_to_step = 320;
    menu_snapshot_host.net_transport =
        std::make_unique<network::NetTransportRuntime>(network::NetTransportRuntime::New());
    menu_snapshot_host.net_transport->capture_outgoing_packets = true;
    std::string menu_snapshot_socket_error;
    if (!menu_snapshot_host.net_transport->socket.Open(0, &menu_snapshot_socket_error)) {
        std::cerr << "join barrier protocol smoke failed opening menu snapshot socket: "
                  << menu_snapshot_socket_error << '\n';
        return false;
    }
    menu_snapshot_host.net_transport->remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39223},
        .last_heard_frame = 0,
        .last_heard_pump_tick = 100,
    });
    menu_snapshot_host.net_session.peers.push_back(network::NetPeerState{
        .player_id = 2,
        .display_name = "Menu Peer",
        .endpoint_address = "127.0.0.1",
        .endpoint_port = 39223,
        .connected = true,
    });
    std::string menu_snapshot_status;
    if (!network::ForceLockstepSnapshotResync(menu_snapshot_host, 2, &menu_snapshot_status)) {
        std::cerr << "join barrier protocol smoke failed queueing menu snapshot: "
                  << menu_snapshot_status << '\n';
        return false;
    }
    StepSingleTick(menu_snapshot_host, menu_snapshot_audio, menu_snapshot_graphics);
    bool menu_snapshot_chunk_sent = false;
    for (const network::UdpPacket& packet : menu_snapshot_host.net_transport->captured_packets) {
        const std::optional<network::SnapshotResyncChunkPacket> decoded_menu_chunk =
            network::TryDecodeSnapshotResyncChunk(packet.bytes.data(), packet.size);
        if (decoded_menu_chunk.has_value() &&
            decoded_menu_chunk->snapshot_frame == 320 &&
            decoded_menu_chunk->chunk_count > 0) {
            menu_snapshot_chunk_sent = true;
            break;
        }
    }
    if (!menu_snapshot_chunk_sent ||
        menu_snapshot_host.net_session.lockstep_snapshot_resync_pending_request ||
        !menu_snapshot_host.net_session.lockstep_snapshot_resync_waiting_for_ack) {
        std::cerr << "join barrier protocol smoke failed: menu fixed tick did not send snapshot chunk\n";
        return false;
    }

    State existing_peer = State::New();
    existing_peer.net_session.role = network::NetRole::Peer;
    existing_peer.net_session.stage_instance_id = host.net_session.stage_instance_id;
    existing_peer.net_session.join_barrier_id = status.barrier_id;
    network::JoinBarrierStatusPacket stale_status = *decoded_status;
    stale_status.barrier_id = status.barrier_id - 1;
    network::HandleJoinBarrierStatus(existing_peer, stale_status);
    if (existing_peer.net_session.join_barrier_active ||
        existing_peer.net_session.join_barrier_id != status.barrier_id) {
        std::cerr << "join barrier protocol smoke failed: stale status changed peer barrier state\n";
        return false;
    }

    network::HandleJoinBarrierStatus(existing_peer, *decoded_status);
    if (!existing_peer.net_session.join_barrier_active ||
        existing_peer.net_session.join_barrier_id != status.barrier_id ||
        existing_peer.net_session.join_barrier_phase != network::JoinBarrierPhase::SendingSnapshot ||
        existing_peer.net_session.join_barrier_active_peer_id != 4 ||
        existing_peer.net_session.join_barrier_queue.size() != 2 ||
        existing_peer.net_session.join_barrier_queue[0] != 4 ||
        existing_peer.net_session.join_barrier_queue[1] != 5 ||
        existing_peer.net_session.join_barrier_transfer_id != 77 ||
        existing_peer.net_session.join_barrier_snapshot_frame != 120 ||
        existing_peer.net_session.join_barrier_chunk_count != 10 ||
        existing_peer.net_session.join_barrier_chunks_done != 4 ||
        existing_peer.net_session.join_barrier_total_bytes != 4096 ||
        existing_peer.net_session.join_barrier_bytes_done != 1600) {
        std::cerr << "join barrier protocol smoke failed: peer did not apply active status\n";
        return false;
    }

    State queued_peer = State::New();
    queued_peer.net_session.role = network::NetRole::Peer;
    queued_peer.net_session.stage_instance_id = host.net_session.stage_instance_id;
    network::HandleJoinBarrierStatus(queued_peer, *decoded_status);
    if (!queued_peer.net_session.join_barrier_active ||
        queued_peer.net_session.join_barrier_active_peer_id != 4 ||
        queued_peer.net_session.join_barrier_queue.size() != 2 ||
        queued_peer.net_session.join_barrier_queue[0] != 4 ||
        queued_peer.net_session.join_barrier_queue[1] != 5) {
        std::cerr << "join barrier protocol smoke failed: queued peer did not observe active catchup\n";
        return false;
    }

    State second_existing_peer = State::New();
    second_existing_peer.net_session.role = network::NetRole::Peer;
    second_existing_peer.net_session.stage_instance_id = host.net_session.stage_instance_id;
    network::HandleJoinBarrierStatus(second_existing_peer, *decoded_status);
    if (!second_existing_peer.net_session.join_barrier_active ||
        second_existing_peer.net_session.join_barrier_active_peer_id != 4 ||
        second_existing_peer.net_session.join_barrier_queue.size() != 2 ||
        second_existing_peer.net_session.join_barrier_queue[0] != 4 ||
        second_existing_peer.net_session.join_barrier_queue[1] != 5) {
        std::cerr << "join barrier protocol smoke failed: second existing peer did not observe active catchup\n";
        return false;
    }

    network::SnapshotResyncAckPacket wrong_ack;
    wrong_ack.stage_instance_id = host.net_session.stage_instance_id;
    wrong_ack.sender_peer_id = 4;
    wrong_ack.transfer_id = 999;
    wrong_ack.snapshot_frame = host.net_session.join_barrier_snapshot_frame;
    wrong_ack.success = 1;
    network::HandleSnapshotResyncAck(host, wrong_ack);
    if (host.net_session.join_barrier_transfer_id != 77 ||
        host.net_session.join_barrier_phase != network::JoinBarrierPhase::SendingSnapshot) {
        std::cerr << "join barrier protocol smoke failed: wrong transfer ack changed host barrier state\n";
        return false;
    }

    network::SnapshotResyncAckPacket ack = wrong_ack;
    ack.transfer_id = host.net_session.join_barrier_transfer_id;
    network::HandleSnapshotResyncAck(host, ack);
    if (host.net_session.join_barrier_transfer_id != 0 ||
        host.net_session.join_barrier_active_peer_id != kInvalidPlayerId ||
        host.net_session.join_barrier_phase != network::JoinBarrierPhase::WaitingForCatchup ||
        host.net_session.join_barrier_queue.size() != 2 ||
        host.net_session.join_barrier_queue[0] != 4 ||
        host.net_session.join_barrier_queue[1] != 5) {
        std::cerr << "join barrier protocol smoke failed: successful ack did not advance queued catchup\n";
        return false;
    }

    host.net_session.join_barrier_active_peer_id = 4;
    host.net_session.join_barrier_phase = network::JoinBarrierPhase::WaitingForAck;
    host.net_session.join_barrier_transfer_id = 78;
    host.net_session.join_barrier_queue.erase(host.net_session.join_barrier_queue.begin());
    ack.sender_peer_id = 4;
    ack.transfer_id = 78;
    network::HandleSnapshotResyncAck(host, ack);
    if (host.net_session.join_barrier_phase != network::JoinBarrierPhase::WaitingForCatchup ||
        host.net_session.join_barrier_transfer_id != 0 ||
        host.net_session.join_barrier_active_peer_id != kInvalidPlayerId ||
        host.net_session.join_barrier_queue.size() != 1 ||
        host.net_session.join_barrier_queue[0] != 5) {
        std::cerr << "join barrier protocol smoke failed: requeued active catchup did not advance\n";
        return false;
    }

    host.net_session.join_barrier_active_peer_id = 5;
    host.net_session.join_barrier_phase = network::JoinBarrierPhase::WaitingForAck;
    host.net_session.join_barrier_transfer_id = 79;
    host.net_session.join_barrier_queue.clear();
    ack.sender_peer_id = 5;
    ack.transfer_id = 79;
    network::HandleSnapshotResyncAck(host, ack);
    if (host.net_session.join_barrier_phase != network::JoinBarrierPhase::ReadyToResume ||
        host.net_session.join_barrier_transfer_id != 0 ||
        host.net_session.join_barrier_active_peer_id != kInvalidPlayerId) {
        std::cerr << "join barrier protocol smoke failed: final ack did not enter ready-to-resume\n";
        return false;
    }

    network::JoinBarrierResumePacket resume;
    resume.stage_instance_id = host.net_session.stage_instance_id;
    resume.sender_peer_id = static_cast<std::uint32_t>(host.net_session.local_player_id);
    resume.barrier_id = host.net_session.join_barrier_id;
    resume.resume_frame = 144;
    const network::EncodedNetPacket encoded_resume = network::EncodeJoinBarrierResume(resume);
    const std::optional<network::JoinBarrierResumePacket> decoded_resume =
        network::TryDecodeJoinBarrierResume(encoded_resume.bytes.data(), encoded_resume.size);
    if (!decoded_resume.has_value() ||
        decoded_resume->stage_instance_id != resume.stage_instance_id ||
        decoded_resume->barrier_id != resume.barrier_id ||
        decoded_resume->resume_frame != resume.resume_frame) {
        std::cerr << "join barrier protocol smoke failed: resume packet roundtrip mismatch\n";
        return false;
    }

    network::JoinBarrierResumePacket stale_resume = *decoded_resume;
    stale_resume.barrier_id = existing_peer.net_session.join_barrier_id - 1;
    network::HandleJoinBarrierResume(existing_peer, stale_resume);
    if (!existing_peer.net_session.join_barrier_active) {
        std::cerr << "join barrier protocol smoke failed: stale resume cleared peer barrier\n";
        return false;
    }

    network::HandleJoinBarrierResume(existing_peer, *decoded_resume);
    network::HandleJoinBarrierResume(queued_peer, *decoded_resume);
    network::HandleJoinBarrierResume(second_existing_peer, *decoded_resume);
    if (existing_peer.net_session.join_barrier_active ||
        queued_peer.net_session.join_barrier_active ||
        second_existing_peer.net_session.join_barrier_active ||
        existing_peer.net_session.lockstep_next_frame_to_step != resume.resume_frame ||
        existing_peer.net_session.lockstep_next_local_input_frame != resume.resume_frame ||
        queued_peer.net_session.lockstep_next_frame_to_step != resume.resume_frame ||
        queued_peer.net_session.lockstep_next_local_input_frame != resume.resume_frame ||
        second_existing_peer.net_session.lockstep_next_frame_to_step != resume.resume_frame ||
        second_existing_peer.net_session.lockstep_next_local_input_frame != resume.resume_frame) {
        std::cerr << "join barrier protocol smoke failed: resume did not clear peer barriers\n";
        return false;
    }

    std::cout << "join barrier protocol smoke ok\n";
    return true;
}

std::vector<network::SnapshotResyncChunkPacket> BuildSnapshotChunksForSmoke(
    const State& state,
    const Graphics& graphics,
    std::uint32_t transfer_id,
    network::LockstepFrame snapshot_frame
) {
    (void)graphics;
    const std::vector<std::uint8_t> bytes =
        SerializeSimSnapshotToBytes(MakeSimSnapshot(state));
    const std::uint32_t chunk_count = static_cast<std::uint32_t>(
        (bytes.size() + network::kNetSnapshotChunkPayloadBytes - 1) /
        network::kNetSnapshotChunkPayloadBytes
    );
    std::vector<network::SnapshotResyncChunkPacket> chunks;
    chunks.reserve(chunk_count);
    for (std::uint32_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
        const std::size_t begin =
            static_cast<std::size_t>(chunk_index) * network::kNetSnapshotChunkPayloadBytes;
        const std::size_t remaining = bytes.size() - begin;
        const std::size_t payload_bytes = std::min<std::size_t>(
            remaining,
            network::kNetSnapshotChunkPayloadBytes
        );
        network::SnapshotResyncChunkPacket chunk;
        chunk.stage_instance_id = state.net_session.stage_instance_id;
        chunk.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);
        chunk.transfer_id = transfer_id;
        chunk.chunk_index = chunk_index;
        chunk.chunk_count = chunk_count;
        chunk.total_bytes = static_cast<std::uint32_t>(bytes.size());
        chunk.payload_bytes = static_cast<std::uint32_t>(payload_bytes);
        chunk.snapshot_frame = snapshot_frame;
        std::copy_n(bytes.data() + begin, payload_bytes, chunk.payload.begin());
        chunks.push_back(chunk);
    }
    return chunks;
}

std::optional<network::SnapshotResyncAckPacket> LastCapturedSnapshotAck(
    const network::NetTransportRuntime& transport
) {
    for (auto it = transport.captured_packets.rbegin();
         it != transport.captured_packets.rend();
         ++it) {
        const std::optional<network::SnapshotResyncAckPacket> ack =
            network::TryDecodeSnapshotResyncAck(it->bytes.data(), it->size);
        if (ack.has_value()) {
            return ack;
        }
    }
    return std::nullopt;
}

bool RunJoinBarrierChunkImpairmentSmoke() {
    Graphics host_graphics;
    Graphics peer_graphics;
    InitCliSmokeRuntimeTables(host_graphics);
    InitCliSmokeRuntimeTables(peer_graphics);

    State host = State::New();
    State peer = State::New();
    const char* failed_step = nullptr;
    if (!PrepareLockstepSmokeState(host, host_graphics, {1}, failed_step) ||
        !PrepareLockstepSmokeState(peer, peer_graphics, {2}, failed_step)) {
        std::cerr << "join barrier chunk impairment smoke failed during setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }

    host.net_session.role = network::NetRole::Host;
    host.net_session.local_player_id = 1;
    host.net_session.stage_instance_id = 88;
    host.net_session.lockstep_next_frame_to_step = 64;
    host.net_session.join_barrier_active = true;
    host.net_session.join_barrier_id = 12;
    host.net_session.join_barrier_phase = network::JoinBarrierPhase::WaitingForAck;
    host.net_session.join_barrier_active_peer_id = 2;
    host.net_session.join_barrier_transfer_id = 444;

    peer.net_session.role = network::NetRole::Peer;
    peer.net_session.local_player_id = 2;
    peer.net_session.stage_instance_id = host.net_session.stage_instance_id;
    peer.net_session.join_barrier_active = true;
    peer.net_session.join_barrier_id = host.net_session.join_barrier_id;
    peer.net_session.join_barrier_phase = network::JoinBarrierPhase::WaitingForCatchup;

    const std::vector<network::SnapshotResyncChunkPacket> chunks =
        BuildSnapshotChunksForSmoke(
            host,
            host_graphics,
            host.net_session.join_barrier_transfer_id,
            host.net_session.lockstep_next_frame_to_step
        );
    if (chunks.size() < 3) {
        std::cerr << "join barrier chunk impairment smoke failed: snapshot too small for reorder/drop coverage\n";
        return false;
    }

    network::NetTransportRuntime peer_transport = network::NetTransportRuntime::New();
    peer_transport.capture_outgoing_packets = true;
    peer_transport.host_endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39000};

    const std::uint32_t missing_index = static_cast<std::uint32_t>(chunks.size() / 2);
    for (std::size_t i = chunks.size(); i-- > 0;) {
        if (i == missing_index) {
            continue;
        }
        network::HandleSnapshotResyncChunk(peer, peer_graphics, peer_transport, chunks[i]);
        if (i == chunks.size() - 1) {
            network::HandleSnapshotResyncChunk(peer, peer_graphics, peer_transport, chunks[i]);
        }
    }

    if (!peer_transport.captured_packets.empty() ||
        peer.net_session.join_barrier_chunks_done >= peer.net_session.join_barrier_chunk_count ||
        peer.net_session.join_barrier_phase == network::JoinBarrierPhase::WaitingForResume) {
        std::cerr << "join barrier chunk impairment smoke failed: incomplete snapshot produced ack/resume\n";
        return false;
    }

    network::HandleSnapshotResyncChunk(
        peer,
        peer_graphics,
        peer_transport,
        chunks[missing_index]
    );
    const std::optional<network::SnapshotResyncAckPacket> first_ack =
        LastCapturedSnapshotAck(peer_transport);
    if (!first_ack.has_value() ||
        first_ack->transfer_id != host.net_session.join_barrier_transfer_id ||
        first_ack->snapshot_frame != host.net_session.lockstep_next_frame_to_step ||
        first_ack->success == 0 ||
        peer.net_session.join_barrier_phase != network::JoinBarrierPhase::WaitingForResume ||
        peer.net_session.join_barrier_chunks_done != peer.net_session.join_barrier_chunk_count ||
        peer.net_session.join_barrier_bytes_done != peer.net_session.join_barrier_total_bytes) {
        std::cerr << "join barrier chunk impairment smoke failed: complete snapshot did not ack/wait for resume\n";
        return false;
    }
    const PlayerSlot* const peer_primary = peer.players.FindPrimaryLocal();
    if (peer_primary == nullptr ||
        peer_primary->player_id != 2 ||
        !peer_primary->ent_vid.has_value() ||
        peer.controlled_ent_vid != peer_primary->ent_vid) {
        std::cerr << "join barrier chunk impairment smoke failed: peer snapshot restore did not preserve local control\n"
                  << "  peer ownership:" << DescribeSmokePlayerOwnership(peer) << '\n';
        return false;
    }
    const std::size_t captured_after_first_ack = peer_transport.captured_packets.size();

    network::HandleSnapshotResyncChunk(
        peer,
        peer_graphics,
        peer_transport,
        chunks.front()
    );
    const std::optional<network::SnapshotResyncAckPacket> resent_ack =
        LastCapturedSnapshotAck(peer_transport);
    if (peer_transport.captured_packets.size() <= captured_after_first_ack ||
        !resent_ack.has_value() ||
        resent_ack->transfer_id != first_ack->transfer_id ||
        resent_ack->snapshot_frame != first_ack->snapshot_frame ||
        resent_ack->success != first_ack->success) {
        std::cerr << "join barrier chunk impairment smoke failed: duplicate chunk did not resend ack\n";
        return false;
    }

    network::HandleSnapshotResyncAck(host, *resent_ack);
    if (host.net_session.join_barrier_phase != network::JoinBarrierPhase::ReadyToResume ||
        host.net_session.join_barrier_transfer_id != 0 ||
        host.net_session.join_barrier_active_peer_id != kInvalidPlayerId) {
        std::cerr << "join barrier chunk impairment smoke failed: host did not accept resent ack\n";
        return false;
    }

    if (!CompareCanonicalFingerprints(host, peer, "join barrier chunk impairment final")) {
        std::cerr << "  first simple diff: " << DescribeFirstStateDifference(host, peer) << '\n';
        return false;
    }

    std::cout << "join barrier chunk impairment smoke ok: chunks="
              << chunks.size() << '\n';
    return true;
}

bool RunJoinBarrierNextStageRestartSmoke() {
    Graphics host_graphics;
    Graphics peer_graphics;
    InitCliSmokeRuntimeTables(host_graphics);
    InitCliSmokeRuntimeTables(peer_graphics);

    State host = State::New();
    State peer = State::New();
    const char* failed_step = nullptr;
    if (!PrepareLockstepSmokeState(host, host_graphics, {1}, failed_step) ||
        !PrepareLockstepSmokeState(peer, peer_graphics, {2}, failed_step)) {
        std::cerr << "join barrier next-stage restart smoke failed during setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }

    host.net_session.role = network::NetRole::Host;
    host.net_session.local_player_id = 1;
    host.net_session.host_player_id = 1;
    host.net_session.stage_instance_id = 101;
    host.net_session.stage_seed = host.stage.generation_seed.value_or(1U);
    host.net_session.lockstep_next_frame_to_step = 0;
    host.net_session.join_barrier_active = true;
    host.net_session.join_barrier_id = 18;
    host.net_session.join_barrier_phase = network::JoinBarrierPhase::WaitingForAck;
    host.net_session.join_barrier_active_peer_id = 2;
    host.net_session.join_barrier_transfer_id = 909;

    peer.net_session.role = network::NetRole::Peer;
    peer.net_session.local_player_id = 2;
    peer.net_session.host_player_id = 1;
    peer.net_session.stage_instance_id = host.net_session.stage_instance_id - 1;
    peer.net_session.run_restart_pending = true;
    peer.net_session.run_restart_transition_queued = true;
    peer.net_session.run_restart_applied_locally = false;
    peer.net_session.run_restart_last_sequence = 1;
    peer.net_session.run_restart_last_packet_stage_instance_id = host.net_session.stage_instance_id;
    peer.net_session.run_restart_stage_seed = host.net_session.stage_seed;
    peer.net_session.run_restart_quest_id = host.stage.quest_id;
    peer.net_session.run_restart_quest_stage_id = host.stage.quest_stage_id;
    peer.net_session.join_barrier_active = true;
    peer.net_session.join_barrier_id = host.net_session.join_barrier_id;
    peer.net_session.join_barrier_phase = network::JoinBarrierPhase::WaitingForCatchup;
    peer.net_session.join_barrier_active_peer_id = 2;

    network::JoinBarrierStatusPacket status;
    status.stage_instance_id = host.net_session.stage_instance_id;
    status.sender_peer_id = 1;
    status.barrier_id = host.net_session.join_barrier_id;
    status.active = 1;
    status.phase = static_cast<std::uint8_t>(network::JoinBarrierPhase::SendingSnapshot);
    status.active_player_id = 2;
    status.transfer_id = host.net_session.join_barrier_transfer_id;
    status.snapshot_frame = host.net_session.lockstep_next_frame_to_step;
    network::HandleJoinBarrierStatus(peer, status);
    if (peer.net_session.join_barrier_transfer_id != host.net_session.join_barrier_transfer_id) {
        std::cerr << "join barrier next-stage restart smoke failed: peer rejected next-stage status\n";
        return false;
    }

    network::NetTransportRuntime peer_transport = network::NetTransportRuntime::New();
    peer_transport.capture_outgoing_packets = true;
    peer_transport.host_endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 39001};

    const std::vector<network::SnapshotResyncChunkPacket> chunks =
        BuildSnapshotChunksForSmoke(
            host,
            host_graphics,
            host.net_session.join_barrier_transfer_id,
            host.net_session.lockstep_next_frame_to_step
        );
    for (const network::SnapshotResyncChunkPacket& chunk : chunks) {
        network::HandleSnapshotResyncChunk(peer, peer_graphics, peer_transport, chunk);
    }

    const std::optional<network::SnapshotResyncAckPacket> ack =
        LastCapturedSnapshotAck(peer_transport);
    if (!ack.has_value() ||
        ack->stage_instance_id != host.net_session.stage_instance_id ||
        ack->transfer_id != host.net_session.join_barrier_transfer_id ||
        ack->success == 0 ||
        peer.net_session.stage_instance_id != host.net_session.stage_instance_id ||
        peer.net_session.stage_seed != host.net_session.stage_seed ||
        peer.net_session.run_restart_pending ||
        peer.net_session.run_restart_transition_queued ||
        peer.net_session.join_barrier_phase != network::JoinBarrierPhase::WaitingForResume) {
        std::cerr << "join barrier next-stage restart smoke failed: next-stage snapshot did not restore/ack\n"
                  << "  ack=" << (ack.has_value() ? "yes" : "no")
                  << " peer_stage=" << peer.net_session.stage_instance_id
                  << " host_stage=" << host.net_session.stage_instance_id
                  << " peer_seed=" << peer.net_session.stage_seed
                  << " host_seed=" << host.net_session.stage_seed
                  << " pending=" << (peer.net_session.run_restart_pending ? "true" : "false")
                  << " queued=" << (peer.net_session.run_restart_transition_queued ? "true" : "false")
                  << " applied=" << (peer.net_session.run_restart_applied_locally ? "true" : "false")
                  << " barrier_phase=" << static_cast<int>(peer.net_session.join_barrier_phase)
                  << '\n';
        return false;
    }

    network::HandleSnapshotResyncAck(host, *ack);
    if (host.net_session.join_barrier_phase != network::JoinBarrierPhase::ReadyToResume) {
        std::cerr << "join barrier next-stage restart smoke failed: host did not accept next-stage ack\n";
        return false;
    }

    network::JoinBarrierResumePacket resume;
    resume.stage_instance_id = host.net_session.stage_instance_id;
    resume.sender_peer_id = 1;
    resume.barrier_id = host.net_session.join_barrier_id;
    resume.resume_frame = host.net_session.lockstep_next_frame_to_step;
    network::HandleJoinBarrierResume(peer, resume);
    if (peer.net_session.join_barrier_active) {
        std::cerr << "join barrier next-stage restart smoke failed: resume did not clear peer barrier\n";
        return false;
    }

    if (!CompareCanonicalFingerprints(host, peer, "join barrier next-stage restart final")) {
        std::cerr << "  first simple diff: " << DescribeFirstStateDifference(host, peer) << '\n';
        return false;
    }

    std::cout << "join barrier next-stage restart smoke ok\n";
    return true;
}

bool AddSmokePlayer(State& state, Graphics& graphics, PlayerId player_id, Vec2 offset) {
    (void)state.players.EnsureLocalPlayer(
        player_id,
        "Player " + std::to_string(player_id),
        false
    );
    Vec2 spawn_pos = Vec2::New(32.0F, 32.0F) + offset;
    if (const PlayerSlot* const primary = state.players.FindPrimaryLocal();
        primary != nullptr && primary->ent_vid.has_value()) {
        if (const Ent* const primary_ent = state.ents.GetEnt(*primary->ent_vid)) {
            spawn_pos = primary_ent->pos + offset;
        }
    }
    const std::optional<VID> player_vid = SpawnPlayerForPlayerId(state, player_id, spawn_pos);
    if (!player_vid.has_value()) {
        return false;
    }
    state.UpdateSidForEnt(player_vid->id, graphics);
    return true;
}

bool RunSimSnapshotMultiLocalOverlaySmoke() {
    Graphics source_graphics;
    Graphics target_graphics;
    InitCliSmokeRuntimeTables(source_graphics);
    InitCliSmokeRuntimeTables(target_graphics);

    State source = State::New();
    State target = State::New();
    const char* failed_step = nullptr;
    if (!PrepareLockstepSmokeState(source, source_graphics, {1}, failed_step) ||
        !PrepareLockstepSmokeState(target, target_graphics, {2}, failed_step) ||
        !AddSmokePlayer(source, source_graphics, 3, Vec2::New(32.0F, 0.0F)) ||
        !AddSmokePlayer(target, target_graphics, 3, Vec2::New(32.0F, 0.0F)) ||
        !ConfigureLockstepSmokeOwnership(source, {1}) ||
        !ConfigureLockstepSmokeOwnership(target, {2, 3})) {
        std::cerr << "sim snapshot multi-local overlay smoke failed during setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }

    const PlayerSlot* const source_primary = source.players.FindPrimaryLocal();
    const PlayerSlot* const target_primary_before = target.players.FindPrimaryLocal();
    if (source_primary == nullptr ||
        source_primary->player_id != 1 ||
        target_primary_before == nullptr ||
        target_primary_before->player_id != 2) {
        std::cerr << "sim snapshot multi-local overlay smoke failed: invalid initial primary slots\n";
        return false;
    }

    const SimSnapshot snapshot = MakeSimSnapshot(source);
    RestoreSimSnapshot(snapshot, target, target_graphics);

    const PlayerSlot* const slot_1 = target.players.Find(1);
    const PlayerSlot* const slot_2 = target.players.Find(2);
    const PlayerSlot* const slot_3 = target.players.Find(3);
    const PlayerSlot* const target_primary_after = target.players.FindPrimaryLocal();
    if (slot_1 == nullptr ||
        slot_2 == nullptr ||
        slot_3 == nullptr ||
        slot_1->connection_kind != PlayerConnectionKind::Remote ||
        slot_1->primary_local ||
        slot_2->connection_kind != PlayerConnectionKind::Local ||
        !slot_2->primary_local ||
        slot_3->connection_kind != PlayerConnectionKind::Local ||
        slot_3->primary_local ||
        target_primary_after == nullptr ||
        target_primary_after->player_id != 2 ||
        !slot_2->ent_vid.has_value() ||
        target.controlled_ent_vid != slot_2->ent_vid) {
        std::cerr << "sim snapshot multi-local overlay smoke failed: local overlay was not preserved\n"
                  << "  ownership:" << DescribeSmokePlayerOwnership(target) << '\n';
        return false;
    }

    std::cout << "sim snapshot multi-local overlay smoke ok\n";
    return true;
}

bool RunRetainedReconnectSmoke() {
    Graphics graphics;
    InitCliSmokeRuntimeTables(graphics);
    State state = State::New();
    const std::vector<PlayerId> players = {1, 2};
    const char* failed_step = nullptr;
    if (!PrepareLockstepSmokeState(state, graphics, players, failed_step)) {
        std::cerr << "retained reconnect smoke failed during setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }

    PlayerSlot* const slot = state.players.Find(2);
    if (slot == nullptr || !slot->ent_vid.has_value()) {
        std::cerr << "retained reconnect smoke failed: missing player slot\n";
        return false;
    }
    Ent* player = state.ents.GetEntMut(*slot->ent_vid);
    if (player == nullptr || !player->active) {
        std::cerr << "retained reconnect smoke failed: missing player ent\n";
        return false;
    }

    player->pos = Vec2::New(128.0F, 192.0F);
    player->health = 277;
    player->money = 54321;
    (void)AddEffect(*player, EffectId::Gloves);
    (void)AddEffect(*player, EffectId::Meathead, 6, 120);
    FillToolSlot(state.ent_tools.EnsureToolSlot(player->vid, 0), ToolKind::ThrowBomb, 11, true);
    FillToolSlot(state.ent_tools.EnsureToolSlot(player->vid, 1), ToolKind::ThrowRope, 7, true);

    Ent* const held = world_ops::SpawnEnt(state, EntType::Rock, [](Ent& ent) {
        ent.pos = Vec2::New(136.0F, 192.0F);
        ent.vel = Vec2::New(1.0F, -2.0F);
        ent.counter_a = 3.0F;
    });
    Ent* const back = world_ops::SpawnEnt(state, EntType::Cape, [](Ent& ent) {
        ent.pos = Vec2::New(120.0F, 192.0F);
        ent.counter_b = 4.0F;
    });
    if (held == nullptr || back == nullptr) {
        std::cerr << "retained reconnect smoke failed: attached ent spawn failed\n";
        return false;
    }
    ents::common::AttachEntAsHeld(*player, *held);
    player->back_vid = back->vid;
    back->held_by_vid = player->vid;
    back->attach_mode = AttachMode::Back;
    back->has_physics = false;
    back->can_collide = false;

    network::StoreRetainedPlayerState(state, *slot, *player);
    const network::NetRetainedPlayerState* const retained =
        network::FindRetainedPlayerState(state, slot->player_id);
    if (retained == nullptr ||
        retained->last_pos != Vec2::New(128.0F, 192.0F) ||
        retained->health != 277 ||
        retained->money != 54321 ||
        !retained->held_item.valid ||
        retained->held_item.ent_type != EntType::Rock ||
        !retained->back_item.valid ||
        retained->back_item.ent_type != EntType::Cape ||
        retained->tool_slots[0].kind != ToolKind::ThrowBomb ||
        retained->tool_slots[0].count != 11 ||
        retained->tool_slots[1].kind != ToolKind::ThrowRope ||
        retained->tool_slots[1].count != 7 ||
        retained->effect_count < 2) {
        std::cerr << "retained reconnect smoke failed: retained state incomplete\n";
        return false;
    }

    network::DeactivateRetainedAttachedEnt(state, retained->held_item, player->holding_vid);
    network::DeactivateRetainedAttachedEnt(state, retained->back_item, player->back_vid);
    if ((state.ents.GetEnt(held->vid) != nullptr && state.ents.GetEnt(held->vid)->active) ||
        (state.ents.GetEnt(back->vid) != nullptr && state.ents.GetEnt(back->vid)->active)) {
        std::cerr << "retained reconnect smoke failed: old attached ents stayed active\n";
        return false;
    }

    player->pos = Vec2::New(16.0F, 16.0F);
    player->health = 1;
    player->money = 2;
    player->holding_vid.reset();
    player->back_vid.reset();
    player->effects.reset();
    if (EntToolState* const tools = state.ent_tools.FindEntToolStateMut(player->vid)) {
        for (ToolSlot& tool_slot : tools->slots) {
            tool_slot = ToolSlot{};
        }
    }

    state.net_session.reconnect_spawn_mode = network::NetReconnectSpawnMode::RetainedAtLastPosition;
    const Vec2 spawn_pos = network::ResolveReconnectSpawnPos(state, retained, 1);
    if (spawn_pos != Vec2::New(128.0F, 192.0F)) {
        std::cerr << "retained reconnect smoke failed: retained spawn pos mismatch\n";
        return false;
    }

    network::ApplyRetainedPlayerState(state, slot->player_id, *retained, spawn_pos, graphics);
    player = state.ents.GetEntMut(*slot->ent_vid);
    if (player == nullptr ||
        player->pos != Vec2::New(128.0F, 192.0F) ||
        player->health != 277 ||
        player->money != 54321 ||
        !HasEffect(*player, EffectId::Gloves) ||
        !HasEffect(*player, EffectId::Meathead)) {
        std::cerr << "retained reconnect smoke failed: player state did not restore\n";
        return false;
    }
    const ToolSlot* const restored_bombs = state.ent_tools.FindToolSlot(player->vid, 0);
    const ToolSlot* const restored_ropes = state.ent_tools.FindToolSlot(player->vid, 1);
    if (restored_bombs == nullptr ||
        restored_bombs->kind != ToolKind::ThrowBomb ||
        restored_bombs->count != 11 ||
        restored_ropes == nullptr ||
        restored_ropes->kind != ToolKind::ThrowRope ||
        restored_ropes->count != 7) {
        std::cerr << "retained reconnect smoke failed: tools did not restore\n";
        return false;
    }
    if (!player->holding_vid.has_value() ||
        !player->back_vid.has_value()) {
        std::cerr << "retained reconnect smoke failed: attached refs did not restore\n";
        return false;
    }
    const Ent* const restored_held = state.ents.GetEnt(*player->holding_vid);
    const Ent* const restored_back = state.ents.GetEnt(*player->back_vid);
    if (restored_held == nullptr ||
        restored_held->type_ != EntType::Rock ||
        restored_held->held_by_vid != player->vid ||
        restored_back == nullptr ||
        restored_back->type_ != EntType::Cape ||
        restored_back->held_by_vid != player->vid ||
        restored_back->attach_mode != AttachMode::Back) {
        std::cerr << "retained reconnect smoke failed: attached ents did not restore\n";
        return false;
    }

    state.net_session.retained_player_lifetime_frames = 10;
    state.frame = static_cast<std::uint32_t>(retained->disconnected_frame + 11);
    network::CleanupExpiredRetainedPlayerStates(state);
    if (network::FindRetainedPlayerState(state, slot->player_id) != nullptr) {
        std::cerr << "retained reconnect smoke failed: expired retained player was kept\n";
        return false;
    }

    std::cout << "retained reconnect smoke ok\n";
    return true;
}

bool RunLockstepHashExchangeSmoke() {
    Graphics graphics;
    InitCliSmokeRuntimeTables(graphics);
    State state = State::New();
    const std::vector<PlayerId> players = {1, 2};
    const char* failed_step = nullptr;
    if (!PrepareLockstepSmokeState(state, graphics, players, failed_step)) {
        std::cerr << "lockstep hash exchange smoke failed during setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }

    const std::vector<std::uint8_t> snapshot_bytes =
        SerializeSimSnapshotToBytes(MakeSimSnapshot(state));
    SimSnapshot decoded_snapshot;
    if (!DeserializeSimSnapshotFromBytes(snapshot_bytes, decoded_snapshot)) {
        std::cerr << "lockstep hash exchange smoke failed: snapshot resync decode failed\n";
        return false;
    }
    Graphics roundtrip_graphics;
    InitCliSmokeRuntimeTables(roundtrip_graphics);
    State roundtrip_state = State::New();
    RestoreSimSnapshot(decoded_snapshot, roundtrip_state, roundtrip_graphics);
    if (!CompareCanonicalFingerprints(state, roundtrip_state, "snapshot resync roundtrip")) {
        return false;
    }

    state.net_session.input_lockstep_enabled = true;
    state.net_session.role = network::NetRole::Peer;
    state.net_session.local_player_id = 2;
    state.net_session.host_player_id = 1;
    state.net_session.stage_instance_id = 99;
    state.net_session.lockstep_next_frame_to_step = 8;
    state.net_session.lockstep_last_confirmed_hash_frame = 3;
    state.net_session.lockstep_last_confirmed_hash = 0x1234ULL;
    state.net_session.lockstep_has_confirmed_hash = true;
    state.net_session.lockstep_hash_history.push_back(network::LockstepHashRecord{
        .frame = 3,
        .hash = 0x1234ULL,
    });
    state.net_session.lockstep_remote_hash_history.push_back(network::LockstepRemoteHashRecord{
        .peer_id = 1,
        .frame = 3,
        .hash = 0x1234ULL,
    });
    state.net_session.lockstep_hash_history.push_back(network::LockstepHashRecord{
        .frame = 4,
        .hash = 0xAAAAULL,
        .component_root = 0xA001ULL,
        .component_stage = 0xA002ULL,
        .component_players = 0xA003ULL,
        .component_tools = 0xA004ULL,
        .component_ents = 0xA005ULL,
    });
    state.net_session.lockstep_rollback_snapshots.push_back(network::LockstepRollbackSnapshot{
        .frame = 4,
        .snapshot = std::make_shared<SimSnapshot>(MakeSimSnapshot(state)),
    });

    network::LockstepHashNetPacket roundtrip;
    roundtrip.stage_instance_id = state.net_session.stage_instance_id;
    roundtrip.sender_peer_id = state.net_session.host_player_id;
    roundtrip.frame = 4;
    roundtrip.hash = 0xBBBBULL;
    roundtrip.component_root = 0xB001ULL;
    roundtrip.component_stage = 0xB002ULL;
    roundtrip.component_players = 0xB003ULL;
    roundtrip.component_tools = 0xB004ULL;
    roundtrip.component_ents = 0xB005ULL;
    const network::EncodedNetPacket encoded = network::EncodeLockstepHash(roundtrip);
    const std::optional<network::LockstepHashNetPacket> decoded =
        network::TryDecodeLockstepHash(encoded.bytes.data(), encoded.size);
    if (!decoded.has_value() ||
        decoded->stage_instance_id != roundtrip.stage_instance_id ||
        decoded->sender_peer_id != roundtrip.sender_peer_id ||
        decoded->frame != roundtrip.frame ||
        decoded->hash != roundtrip.hash ||
        decoded->component_root != roundtrip.component_root ||
        decoded->component_stage != roundtrip.component_stage ||
        decoded->component_players != roundtrip.component_players ||
        decoded->component_tools != roundtrip.component_tools ||
        decoded->component_ents != roundtrip.component_ents) {
        std::cerr << "lockstep hash exchange smoke failed: packet roundtrip mismatch\n";
        return false;
    }

    network::HandleLockstepHashPacket(state, *decoded);
    if (state.net_session.lockstep_hash_mismatch_count != 1 ||
        state.net_session.lockstep_last_mismatch_frame != 4 ||
        state.net_session.lockstep_last_mismatch_local_hash != 0xAAAAULL ||
        state.net_session.lockstep_last_mismatch_remote_hash != 0xBBBBULL ||
        state.net_session.lockstep_last_mismatch_local_stage != 0xA002ULL ||
        state.net_session.lockstep_last_mismatch_remote_stage != 0xB002ULL ||
        state.net_session.lockstep_last_mismatch_local_ents != 0xA005ULL ||
        state.net_session.lockstep_last_mismatch_remote_ents != 0xB005ULL ||
        state.net_session.lockstep_last_mismatch_local_ent_hashes.empty() ||
        state.net_session.lockstep_last_desync_recovery_mode !=
            network::LockstepDesyncRecoveryMode::PendingRollback ||
        !state.net_session.lockstep_rollback_requested_frame.has_value() ||
        *state.net_session.lockstep_rollback_requested_frame != 4) {
        std::cerr << "lockstep hash exchange smoke failed: mismatch did not request rollback\n";
        return false;
    }

    state.net_session.lockstep_rollback_requested_frame = std::nullopt;
    state.net_session.lockstep_rollback_snapshots.clear();
    state.net_session.lockstep_rollback_snapshots.push_back(network::LockstepRollbackSnapshot{
        .frame = 0,
        .snapshot = std::make_shared<SimSnapshot>(MakeSimSnapshot(state)),
    });
    network::LockstepHashNetPacket catchup_packet = roundtrip;
    catchup_packet.hash = 0xCCCCULL;
    network::HandleLockstepHashPacket(state, catchup_packet);
    if (state.net_session.lockstep_last_desync_recovery_mode !=
            network::LockstepDesyncRecoveryMode::SnapshotCatchup ||
        !state.net_session.lockstep_snapshot_resync_pending_request ||
        state.net_session.lockstep_snapshot_resync_target_peer_id != state.net_session.host_player_id ||
        state.net_session.lockstep_rollback_requested_frame.has_value()) {
        std::cerr << "lockstep hash exchange smoke failed: old mismatch did not request snapshot catchup\n";
        return false;
    }

    state.net_session.lockstep_snapshot_resync_pending_request = false;
    state.net_session.lockstep_snapshot_resync_target_peer_id = kInvalidPlayerId;
    state.net_session.lockstep_last_desync_recovery_mode =
        network::LockstepDesyncRecoveryMode::None;
    state.net_session.lockstep_rollback_snapshots.clear();
    network::LockstepHashNetPacket fatal_packet = roundtrip;
    fatal_packet.hash = 0xDDDDULL;
    network::HandleLockstepHashPacket(state, fatal_packet);
    if (state.net_session.lockstep_last_desync_recovery_mode !=
            network::LockstepDesyncRecoveryMode::SnapshotCatchup ||
        !state.net_session.lockstep_snapshot_resync_pending_request ||
        state.net_session.lockstep_snapshot_resync_target_peer_id != state.net_session.host_player_id) {
        std::cerr << "lockstep hash exchange smoke failed: missing rollback history did not request host snapshot\n";
        return false;
    }

    std::cout << "lockstep hash exchange smoke ok\n";
    return true;
}

bool RunLockstepStageTransitionResyncBlockSmoke() {
    Graphics graphics;
    InitCliSmokeRuntimeTables(graphics);
    Audio audio;
    State state = State::New();
    if (!LoadQuestStage(state, "classic", "classic_mines_1", false, 12345U)) {
        std::cerr << "lockstep stage transition resync-block smoke failed: load stage\n";
        return false;
    }

    std::string status;
    if (!network::StartHostSession(state, 0, network::kDefaultLockstepInputDelayFrames, &status)) {
        std::cerr << "lockstep stage transition resync-block smoke failed: "
                  << status << '\n';
        return false;
    }
    state.net_transport->remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 9},
        .last_heard_frame = state.frame,
    });

    const StageTransitionTarget transition{
        .destination = StageLoadTarget::ForQuestStage("classic", "classic_mines_2"),
        .preserve_player_state = true,
        .seed = 24680U,
    };
    QueueStageTransition(state, transition);
    state.SetMode(Mode::StageTransition);
    state.scene_frame = 0;
    state.net_session.lockstep_last_desync_recovery_mode =
        network::LockstepDesyncRecoveryMode::SnapshotCatchup;
    state.net_session.lockstep_snapshot_resync_waiting_for_ack = true;
    state.net_session.lockstep_snapshot_resync_active_transfer_id = 1;
    state.net_session.lockstep_snapshot_resync_target_peer_id = 2;

    for (std::uint32_t i = 0; i < 70; ++i) {
        StepSingleTick(state, audio, graphics);
    }

    if (state.stage.quest_stage_id != "classic_mines_1" ||
        state.mode != Mode::StageTransition ||
        state.net_session.lockstep_last_desync_recovery_mode !=
            network::LockstepDesyncRecoveryMode::SnapshotCatchup) {
        std::cerr << "lockstep stage transition resync-block smoke failed:"
                  << " transition advanced while snapshot catchup was active,"
                  << " stage=" << state.stage.quest_stage_id
                  << " mode=" << static_cast<int>(state.mode)
                  << " recovery="
                  << static_cast<int>(state.net_session.lockstep_last_desync_recovery_mode)
                  << '\n';
        return false;
    }

    state.net_session.lockstep_last_desync_recovery_mode =
        network::LockstepDesyncRecoveryMode::None;
    state.net_session.lockstep_snapshot_resync_waiting_for_ack = false;
    state.net_session.lockstep_snapshot_resync_active_transfer_id = 0;
    state.net_session.lockstep_snapshot_resync_target_peer_id = kInvalidPlayerId;
    state.net_session.lockstep_snapshot_resync_bytes.clear();

    for (std::uint32_t i = 0; i < 70; ++i) {
        StepSingleTickWithMode(state, audio, graphics, SimulationTickMode::ReplayNoNetwork);
    }

    if (state.stage.quest_stage_id != "classic_mines_2" ||
        state.mode != Mode::Playing ||
        state.net_session.lockstep_last_desync_recovery_mode !=
            network::LockstepDesyncRecoveryMode::None) {
        std::cerr << "lockstep stage transition resync-block smoke failed:"
                  << " stage=" << state.stage.quest_stage_id
                  << " mode=" << static_cast<int>(state.mode)
                  << " recovery="
                  << static_cast<int>(state.net_session.lockstep_last_desync_recovery_mode)
                  << '\n';
        return false;
    }

    std::cout << "lockstep stage transition resync-block smoke ok\n";
    return true;
}

bool RunLockstepHashRollbackRepairSmoke() {
    Graphics truth_graphics;
    Graphics repaired_graphics;
    InitCliSmokeRuntimeTables(truth_graphics);
    InitCliSmokeRuntimeTables(repaired_graphics);
    Audio truth_audio;
    Audio repaired_audio;

    State truth = State::New();
    State repaired = State::New();
    const std::vector<PlayerId> players = {1, 2};
    const char* failed_step = nullptr;
    if (!PrepareLockstepSmokeState(truth, truth_graphics, {1}, failed_step) ||
        !PrepareLockstepSmokeState(repaired, repaired_graphics, {1}, failed_step)) {
        std::cerr << "lockstep hash rollback repair smoke failed during setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }

    truth.net_session.role = network::NetRole::Peer;
    truth.net_session.local_player_id = 1;
    truth.net_session.stage_instance_id = 0xBEEFU;
    repaired.net_session.role = network::NetRole::Peer;
    repaired.net_session.local_player_id = 1;
    repaired.net_session.stage_instance_id = truth.net_session.stage_instance_id;
    network::RegisterStageEntLinks(truth);
    network::RegisterStageEntLinks(repaired);

    InputFrame neutral = InputFrame::New();
    InputFrame remote_actual = InputFrame::New();
    remote_actual.right = true;
    remote_actual.run = true;

    ApplyLockstepInputsToState(truth, players, {neutral, neutral});
    StepSingleTickWithMode(truth, truth_audio, truth_graphics, SimulationTickMode::ReplayNoNetwork);
    ApplyLockstepInputsToState(repaired, players, {neutral, neutral});
    StepSingleTickWithMode(
        repaired,
        repaired_audio,
        repaired_graphics,
        SimulationTickMode::ReplayNoNetwork
    );

    const std::uint64_t matching_hash =
        ComputeNetworkStateFingerprint(truth).value;
    if (matching_hash != ComputeNetworkStateFingerprint(repaired).value) {
        std::cerr << "lockstep hash rollback repair smoke failed: setup diverged early\n";
        return false;
    }

    repaired.net_session.lockstep_next_frame_to_step = 1;
    repaired.net_session.lockstep_rollback_snapshots.push_back(network::LockstepRollbackSnapshot{
        .frame = 1,
        .snapshot = std::make_shared<SimSnapshot>(MakeSimSnapshot(repaired)),
    });
    repaired.net_session.lockstep_hash_history.push_back(network::LockstepHashRecord{
        .frame = 0,
        .hash = matching_hash,
    });
    repaired.net_session.lockstep_remote_hash_history.push_back(
        network::LockstepRemoteHashRecord{
            .peer_id = 2,
            .frame = 0,
            .hash = matching_hash,
        }
    );
    repaired.net_session.lockstep_last_confirmed_hash_frame = 0;
    repaired.net_session.lockstep_last_confirmed_hash = matching_hash;
    repaired.net_session.lockstep_has_confirmed_hash = true;

    network::LockstepInputRecord player_1_frame_1;
    player_1_frame_1.player_id = 1;
    player_1_frame_1.frame = 1;
    player_1_frame_1.input = neutral;
    player_1_frame_1.canonical = true;
    (void)repaired.net_session.lockstep_input_buffer.Store(player_1_frame_1);
    network::LockstepInputRecord player_2_frame_1;
    player_2_frame_1.player_id = 2;
    player_2_frame_1.frame = 1;
    player_2_frame_1.input = remote_actual;
    player_2_frame_1.canonical = true;
    (void)repaired.net_session.lockstep_input_buffer.Store(player_2_frame_1);

    ApplyLockstepInputsToState(truth, players, {neutral, remote_actual});
    StepSingleTickWithMode(truth, truth_audio, truth_graphics, SimulationTickMode::ReplayNoNetwork);
    truth.net_session.lockstep_next_frame_to_step = 2;
    ApplyLockstepInputsToState(repaired, players, {neutral, neutral});
    StepSingleTickWithMode(
        repaired,
        repaired_audio,
        repaired_graphics,
        SimulationTickMode::ReplayNoNetwork
    );
    repaired.net_session.lockstep_next_frame_to_step = 2;
    repaired.points += 1;

    const std::uint64_t truth_hash = ComputeNetworkStateFingerprint(truth).value;
    const std::uint64_t repaired_bad_hash =
        ComputeNetworkStateFingerprint(repaired).value;
    if (truth_hash == repaired_bad_hash) {
        std::cerr << "lockstep hash rollback repair smoke failed: perturbation did not diverge\n";
        return false;
    }
    repaired.net_session.lockstep_hash_history.push_back(network::LockstepHashRecord{
        .frame = 1,
        .hash = repaired_bad_hash,
    });

    network::LockstepHashNetPacket mismatch;
    mismatch.stage_instance_id = repaired.net_session.stage_instance_id;
    mismatch.sender_peer_id = 2;
    mismatch.frame = 1;
    mismatch.hash = truth_hash;
    network::HandleLockstepHashPacket(repaired, mismatch);
    if (repaired.net_session.lockstep_last_desync_recovery_mode !=
            network::LockstepDesyncRecoveryMode::PendingRollback ||
        !repaired.net_session.lockstep_rollback_requested_frame.has_value() ||
        *repaired.net_session.lockstep_rollback_requested_frame != 1) {
        std::cerr << "lockstep hash rollback repair smoke failed: mismatch did not request frame 1 rollback\n";
        return false;
    }

    if (!network::ReplayPendingInputLockstepRollback(repaired, repaired_graphics)) {
        std::cerr << "lockstep hash rollback repair smoke failed: rollback replay failed\n";
        return false;
    }
    if (repaired.net_session.lockstep_last_desync_recovery_mode !=
        network::LockstepDesyncRecoveryMode::RollbackRepaired) {
        const NetworkStateFingerprintComponents truth_components =
            ComputeNetworkStateFingerprintComponents(truth);
        const NetworkStateFingerprintComponents repaired_components =
            ComputeNetworkStateFingerprintComponents(repaired);
        std::cerr << "lockstep hash rollback repair smoke failed: replay did not mark repaired"
                  << " mode="
                  << static_cast<int>(repaired.net_session.lockstep_last_desync_recovery_mode)
                  << " local_hashes=" << repaired.net_session.lockstep_hash_history.size()
                  << " remote_hashes=" << repaired.net_session.lockstep_remote_hash_history.size()
                  << " pending_remote_hashes="
                  << repaired.net_session.lockstep_pending_remote_hashes.size()
                  << "\n  truth components root=" << truth_components.root
                  << " stage=" << truth_components.stage
                  << " players=" << truth_components.players
                  << " tools=" << truth_components.tools
                  << " ents=" << truth_components.ents
                  << "\n  repaired components root=" << repaired_components.root
                  << " stage=" << repaired_components.stage
                  << " players=" << repaired_components.players
                  << " tools=" << repaired_components.tools
                  << " ents=" << repaired_components.ents
                  << "\n  first simple diff: "
                  << DescribeFirstStateDifference(truth, repaired) << '\n';
        return false;
    }
    if (!CompareCanonicalFingerprints(truth, repaired, "lockstep hash rollback repair final")) {
        std::cerr << "  first simple diff: "
                  << DescribeFirstStateDifference(truth, repaired) << '\n';
        return false;
    }

    std::cout << "lockstep hash rollback repair smoke ok\n";
    return true;
}

struct RollbackSmokeSnapshot {
    network::LockstepFrame frame = 0;
    SimSnapshot snapshot;
};

const SimSnapshot* FindRollbackSmokeSnapshot(
    const std::vector<RollbackSmokeSnapshot>& snapshots,
    network::LockstepFrame frame
) {
    for (const RollbackSmokeSnapshot& entry : snapshots) {
        if (entry.frame == frame) {
            return &entry.snapshot;
        }
    }
    return nullptr;
}

void SaveRollbackSmokeSnapshot(
    std::vector<RollbackSmokeSnapshot>& snapshots,
    network::LockstepFrame frame,
    const State& state,
    const Graphics& graphics
) {
    (void)graphics;
    for (RollbackSmokeSnapshot& entry : snapshots) {
        if (entry.frame == frame) {
            entry.snapshot = MakeSimSnapshot(state);
            return;
        }
    }
    snapshots.push_back(RollbackSmokeSnapshot{
        .frame = frame,
        .snapshot = MakeSimSnapshot(state),
    });
}

InputFrame PredictRollbackSmokeInput(
    const network::LockstepInputBuffer& buffer,
    PlayerId player_id,
    network::LockstepFrame frame
) {
    const network::LockstepInputRecord* const latest =
        buffer.FindLatestRecordBefore(player_id, frame);
    return latest != nullptr ? latest->input : InputFrame::New();
}

bool BuildRollbackSmokeInputs(
    network::LockstepInputBuffer& buffer,
    network::LockstepFrame frame,
    std::vector<InputFrame>& out_inputs
) {
    out_inputs.clear();
    out_inputs.reserve(2);
    for (PlayerId player_id : {1U, 2U}) {
        const InputFrame* input = buffer.Find(player_id, frame);
        if (input == nullptr && player_id == 2U) {
            network::LockstepInputRecord predicted;
            predicted.player_id = player_id;
            predicted.frame = frame;
            predicted.input = PredictRollbackSmokeInput(buffer, player_id, frame);
            predicted.predicted = true;
            (void)buffer.Store(predicted);
            input = buffer.Find(player_id, frame);
        }
        if (input == nullptr) {
            return false;
        }
        out_inputs.push_back(*input);
    }
    return true;
}

bool RunRollbackLatencySmoke() {
    constexpr network::LockstepFrame kFrames = 240;

    Graphics truth_graphics;
    Graphics predicted_graphics;
    InitCliSmokeRuntimeTables(truth_graphics);
    InitCliSmokeRuntimeTables(predicted_graphics);
    Audio truth_audio;
    Audio predicted_audio;

    State truth = State::New();
    State predicted = State::New();
    const std::vector<PlayerId> players = {1, 2};
    const char* failed_step = nullptr;
    if (!PrepareLockstepSmokeState(truth, truth_graphics, players, failed_step) ||
        !PrepareLockstepSmokeState(predicted, predicted_graphics, players, failed_step)) {
        std::cerr << "rollback latency smoke failed during setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }

    const std::vector<std::array<InputFrame, 2>> script = BuildInputLockstepSmokeScript();
    network::LockstepInputBuffer buffer;
    std::vector<RollbackSmokeSnapshot> snapshots;
    std::optional<network::LockstepFrame> rollback_frame;
    std::vector<InputFrame> frame_inputs;
    network::LockstepFrame predicted_next_frame = 0;
    std::uint32_t rollback_count = 0;
    std::uint32_t prediction_miss_count = 0;
    std::uint32_t prediction_late_match_count = 0;

    struct Delivery {
        network::LockstepFrame due_tick = 0;
        network::LockstepFrame input_frame = 0;
    };
    std::vector<Delivery> deliveries;
    for (network::LockstepFrame frame = 0; frame < kFrames; ++frame) {
        const network::LockstepFrame delay =
            2 + static_cast<network::LockstepFrame>((frame * 7U) % 6U);
        deliveries.push_back(Delivery{.due_tick = frame + delay, .input_frame = frame});
        if ((frame % 17U) == 0U) {
            deliveries.push_back(Delivery{
                .due_tick = frame + delay + 1U,
                .input_frame = frame,
            });
        }
    }
    std::sort(
        deliveries.begin(),
        deliveries.end(),
        [](const Delivery& lhs, const Delivery& rhs) {
            if (lhs.due_tick != rhs.due_tick) {
                return lhs.due_tick < rhs.due_tick;
            }
            return lhs.input_frame > rhs.input_frame;
        }
    );

    std::size_t next_delivery = 0;
    for (network::LockstepFrame frame = 0; frame < kFrames; ++frame) {
        ApplyLockstepInputsToState(truth, players, {
            script[static_cast<std::size_t>(frame)][0],
            script[static_cast<std::size_t>(frame)][1],
        });
        StepSingleTickWithMode(truth, truth_audio, truth_graphics, SimulationTickMode::ReplayNoNetwork);
    }

    for (network::LockstepFrame wall_tick = 0; wall_tick < kFrames + 16U; ++wall_tick) {
        if (wall_tick < kFrames) {
            network::LockstepInputRecord local;
            local.player_id = 1;
            local.frame = wall_tick;
            local.input = script[static_cast<std::size_t>(wall_tick)][0];
            (void)buffer.Store(local);
        }

        while (next_delivery < deliveries.size() && deliveries[next_delivery].due_tick <= wall_tick) {
            const network::LockstepFrame frame = deliveries[next_delivery].input_frame;
            network::LockstepInputRecord remote;
            remote.player_id = 2;
            remote.frame = frame;
            remote.input = script[static_cast<std::size_t>(frame)][1];
            const network::LockstepInputStoreResult result = buffer.Store(remote);
            if (result.replaced_prediction) {
                if (result.mismatch_frame.has_value()) {
                    prediction_miss_count += 1;
                } else {
                    prediction_late_match_count += 1;
                }
            }
            if (result.mismatch_frame.has_value() && *result.mismatch_frame < predicted_next_frame) {
                if (!rollback_frame.has_value() || *result.mismatch_frame < *rollback_frame) {
                    rollback_frame = *result.mismatch_frame;
                }
            }
            ++next_delivery;
        }

        if (rollback_frame.has_value()) {
            const network::LockstepFrame target = predicted_next_frame;
            const SimSnapshot* const snapshot =
                FindRollbackSmokeSnapshot(snapshots, *rollback_frame);
            if (snapshot == nullptr) {
                std::cerr << "rollback latency smoke failed: missing snapshot\n";
                return false;
            }
            RestoreSimSnapshot(*snapshot, predicted, predicted_graphics);
            predicted_next_frame = *rollback_frame;
            rollback_frame = std::nullopt;
            rollback_count += 1;
            while (predicted_next_frame < target) {
                if (!BuildRollbackSmokeInputs(buffer, predicted_next_frame, frame_inputs)) {
                    std::cerr << "rollback latency smoke failed during replay input build\n";
                    return false;
                }
                SaveRollbackSmokeSnapshot(
                    snapshots,
                    predicted_next_frame,
                    predicted,
                    predicted_graphics
                );
                ApplyLockstepInputsToState(predicted, players, frame_inputs);
                StepSingleTickWithMode(
                    predicted,
                    predicted_audio,
                    predicted_graphics,
                    SimulationTickMode::ReplayNoNetwork
                );
                predicted_next_frame += 1;
            }
        }

        if (predicted_next_frame < kFrames) {
            if (!BuildRollbackSmokeInputs(buffer, predicted_next_frame, frame_inputs)) {
                continue;
            }
            SaveRollbackSmokeSnapshot(
                snapshots,
                predicted_next_frame,
                predicted,
                predicted_graphics
            );
            ApplyLockstepInputsToState(predicted, players, frame_inputs);
            StepSingleTickWithMode(
                predicted,
                predicted_audio,
                predicted_graphics,
                SimulationTickMode::ReplayNoNetwork
            );
            predicted_next_frame += 1;
        }
    }

    if (predicted_next_frame != kFrames || next_delivery != deliveries.size()) {
        std::cerr << "rollback latency smoke failed: did not finish all frames/deliveries\n";
        return false;
    }
    if (rollback_count == 0) {
        std::cerr << "rollback latency smoke failed: no rollback was exercised\n";
        return false;
    }
    if (prediction_miss_count == 0) {
        std::cerr << "rollback latency smoke failed: prediction misses were not tracked\n";
        return false;
    }
    if (prediction_miss_count + prediction_late_match_count == 0) {
        std::cerr << "rollback latency smoke failed: no predicted input resolutions were tracked\n";
        return false;
    }
    if (buffer.PredictedRecordCount() != 0) {
        std::cerr << "rollback latency smoke failed: predicted inputs were left unresolved\n";
        return false;
    }
    if (!CompareCanonicalFingerprints(truth, predicted, "rollback latency final")) {
        std::cerr << "  first simple diff: "
                  << DescribeFirstStateDifference(truth, predicted) << '\n';
        return false;
    }

    std::cout << "rollback latency smoke ok: frames=" << kFrames
              << " rollbacks=" << rollback_count
              << " prediction_misses=" << prediction_miss_count
              << " prediction_late_matches=" << prediction_late_match_count << '\n';
    return true;
}

bool RunLockstepSettingsScheduleSmoke() {
    Graphics host_graphics;
    Graphics peer_graphics;
    InitCliSmokeRuntimeTables(host_graphics);
    InitCliSmokeRuntimeTables(peer_graphics);

    State host = State::New();
    State peer = State::New();
    const char* failed_step = nullptr;
    if (!PrepareLockstepSmokeState(host, host_graphics, {1}, failed_step) ||
        !PrepareLockstepSmokeState(peer, peer_graphics, {2}, failed_step)) {
        std::cerr << "lockstep settings smoke failed during setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }
    host.net_session.role = network::NetRole::Host;
    peer.net_session.role = network::NetRole::Peer;
    host.net_session.stage_instance_id = 77;
    peer.net_session.stage_instance_id = 77;
    host.net_session.lockstep_next_frame_to_step = 100;
    peer.net_session.lockstep_next_frame_to_step = 100;
    host.net_session.lockstep_input_delay_frames = 2;
    peer.net_session.lockstep_input_delay_frames = 2;
    host.net_session.lockstep_max_rollback_frames = 12;
    peer.net_session.lockstep_max_rollback_frames = 12;

    std::string status;
    if (!network::ScheduleLockstepSettingsChange(host, 5, 24, &status)) {
        std::cerr << "lockstep settings smoke failed scheduling: " << status << '\n';
        return false;
    }
    if (!host.net_session.lockstep_pending_settings.has_value()) {
        std::cerr << "lockstep settings smoke failed: host has no pending settings\n";
        return false;
    }
    if (!host.net_session.lockstep_broadcast_settings.has_value()) {
        std::cerr << "lockstep settings smoke failed: host has no broadcast settings\n";
        return false;
    }
    const network::PendingLockstepSettings pending =
        *host.net_session.lockstep_pending_settings;
    if (pending.apply_frame <= host.net_session.lockstep_next_frame_to_step ||
        pending.input_delay_frames != 5 ||
        pending.max_rollback_frames != 24 ||
        host.net_session.lockstep_input_delay_frames != 2 ||
        host.net_session.lockstep_max_rollback_frames != 12) {
        std::cerr << "lockstep settings smoke failed: host pending settings are invalid\n";
        return false;
    }
    if (host.net_session.lockstep_broadcast_settings->sequence != pending.sequence ||
        host.net_session.lockstep_broadcast_settings_until_frame <= pending.apply_frame) {
        std::cerr << "lockstep settings smoke failed: broadcast retention is invalid\n";
        return false;
    }

    network::LockstepSettingsPacket packet;
    packet.stage_instance_id = host.net_session.stage_instance_id;
    packet.sender_peer_id = host.net_session.local_player_id;
    packet.sequence = pending.sequence;
    packet.apply_frame = pending.apply_frame;
    packet.input_delay_frames = pending.input_delay_frames;
    packet.max_rollback_frames = pending.max_rollback_frames;
    const network::EncodedNetPacket encoded = network::EncodeLockstepSettings(packet);
    const std::optional<network::LockstepSettingsPacket> decoded =
        network::TryDecodeLockstepSettings(encoded.bytes.data(), encoded.size);
    if (!decoded.has_value()) {
        std::cerr << "lockstep settings smoke failed: settings packet did not decode\n";
        return false;
    }
    network::HandleLockstepSettingsPacket(peer, *decoded);
    if (!peer.net_session.lockstep_pending_settings.has_value() ||
        peer.net_session.lockstep_pending_settings->sequence != pending.sequence ||
        peer.net_session.lockstep_pending_settings->apply_frame != pending.apply_frame) {
        std::cerr << "lockstep settings smoke failed: peer did not store pending settings\n";
        return false;
    }

    host.net_session.lockstep_next_frame_to_step = pending.apply_frame - 1;
    peer.net_session.lockstep_next_frame_to_step = pending.apply_frame - 1;
    network::ApplyDueLockstepSettings(host);
    network::ApplyDueLockstepSettings(peer);
    if (host.net_session.lockstep_input_delay_frames != 2 ||
        peer.net_session.lockstep_input_delay_frames != 2) {
        std::cerr << "lockstep settings smoke failed: settings applied too early\n";
        return false;
    }

    host.net_session.lockstep_next_frame_to_step = pending.apply_frame;
    peer.net_session.lockstep_next_frame_to_step = pending.apply_frame;
    network::ApplyDueLockstepSettings(host);
    network::ApplyDueLockstepSettings(peer);
    if (host.net_session.lockstep_input_delay_frames != 5 ||
        peer.net_session.lockstep_input_delay_frames != 5 ||
        host.net_session.lockstep_max_rollback_frames != 24 ||
        peer.net_session.lockstep_max_rollback_frames != 24 ||
        host.net_session.lockstep_pending_settings.has_value() ||
        peer.net_session.lockstep_pending_settings.has_value()) {
        std::cerr << "lockstep settings smoke failed: settings did not apply consistently\n";
        return false;
    }
    if (!host.net_session.lockstep_broadcast_settings.has_value()) {
        std::cerr << "lockstep settings smoke failed: host stopped rebroadcasting at apply\n";
        return false;
    }
    host.net_session.lockstep_next_frame_to_step =
        host.net_session.lockstep_broadcast_settings_until_frame + 1;
    network::ApplyDueLockstepSettings(host);
    if (host.net_session.lockstep_broadcast_settings.has_value()) {
        std::cerr << "lockstep settings smoke failed: host retained broadcast settings too long\n";
        return false;
    }

    host.net_session.lockstep_next_frame_to_step = pending.apply_frame + 20;
    peer.net_session.lockstep_next_frame_to_step = pending.apply_frame + 20;
    if (!network::ScheduleLockstepSettingsChange(host, 4, 6, &status)) {
        std::cerr << "lockstep settings smoke failed scheduling decrease: " << status << '\n';
        return false;
    }
    const network::PendingLockstepSettings decrease =
        *host.net_session.lockstep_pending_settings;
    network::LockstepSettingsPacket decrease_packet;
    decrease_packet.stage_instance_id = host.net_session.stage_instance_id;
    decrease_packet.sender_peer_id = host.net_session.local_player_id;
    decrease_packet.sequence = decrease.sequence;
    decrease_packet.apply_frame = decrease.apply_frame;
    decrease_packet.input_delay_frames = decrease.input_delay_frames;
    decrease_packet.max_rollback_frames = decrease.max_rollback_frames;
    const network::EncodedNetPacket decrease_encoded =
        network::EncodeLockstepSettings(decrease_packet);
    const std::optional<network::LockstepSettingsPacket> decrease_decoded =
        network::TryDecodeLockstepSettings(decrease_encoded.bytes.data(), decrease_encoded.size);
    if (!decrease_decoded.has_value()) {
        std::cerr << "lockstep settings smoke failed: decrease packet did not decode\n";
        return false;
    }
    network::HandleLockstepSettingsPacket(peer, *decrease_decoded);
    host.net_session.lockstep_next_frame_to_step = decrease.apply_frame;
    peer.net_session.lockstep_next_frame_to_step = decrease.apply_frame;
    network::ApplyDueLockstepSettings(host);
    network::ApplyDueLockstepSettings(peer);
    if (host.net_session.lockstep_input_delay_frames != 4 ||
        peer.net_session.lockstep_input_delay_frames != 4 ||
        host.net_session.lockstep_max_rollback_frames != 6 ||
        peer.net_session.lockstep_max_rollback_frames != 6) {
        std::cerr << "lockstep settings smoke failed: decrease did not apply consistently\n";
        return false;
    }

    State auto_host = State::New();
    Graphics auto_graphics;
    InitCliSmokeRuntimeTables(auto_graphics);
    if (!PrepareLockstepSmokeState(auto_host, auto_graphics, {1}, failed_step)) {
        std::cerr << "lockstep settings smoke failed during auto setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }
    auto_host.net_session.role = network::NetRole::Host;
    auto_host.net_session.input_lockstep_enabled = true;
    auto_host.net_session.lockstep_input_delay_frames = 2;
    auto_host.net_session.lockstep_max_rollback_frames = 12;
    auto_host.net_session.lockstep_auto_delay_enabled = true;
    network::NetPeerState remote;
    remote.player_id = 2;
    remote.connected = true;
    remote.estimated_ping_ms = 150.0F;
    remote.jitter_ms = 20.0F;
    auto_host.net_session.peers.push_back(remote);
    for (std::uint32_t i = 0; i < network::kLockstepAutoDelayStableFrames; ++i) {
        network::UpdateLockstepAutoDelay(auto_host);
    }
    if (!auto_host.net_session.lockstep_pending_settings.has_value()) {
        std::cerr << "lockstep settings smoke failed: auto delay did not schedule\n";
        return false;
    }
    const network::PendingLockstepSettings auto_pending =
        *auto_host.net_session.lockstep_pending_settings;
    if (auto_pending.input_delay_frames <= 2 ||
        auto_pending.max_rollback_frames != 12 ||
        auto_pending.apply_frame <= auto_host.net_session.lockstep_next_frame_to_step) {
        std::cerr << "lockstep settings smoke failed: auto delay pending settings invalid\n";
        return false;
    }

    std::cout << "lockstep settings smoke ok: apply_frame=" << pending.apply_frame
              << " delay=" << host.net_session.lockstep_input_delay_frames
              << " rollback=" << host.net_session.lockstep_max_rollback_frames
              << " auto_delay=" << auto_pending.input_delay_frames << '\n';
    return true;
}

bool RunHostWaitsForMissingInputSmoke() {
    Graphics host_graphics;
    InitCliSmokeRuntimeTables(host_graphics);

    Audio host_audio;
    State host = State::New();
    const char* failed_step = nullptr;

    if (!PrepareLockstepSmokeState(host, host_graphics, {1}, failed_step)) {
        std::cerr << "host waits for missing input smoke failed during setup: "
                  << (failed_step != nullptr ? failed_step : "unknown") << '\n';
        return false;
    }

    host.net_transport =
        std::make_unique<network::NetTransportRuntime>(network::NetTransportRuntime::New());
    std::string socket_error;
    if (!host.net_transport->socket.Open(0, &socket_error)) {
        std::cerr << "host waits for missing input smoke failed opening UDP socket: "
                  << socket_error << '\n';
        return false;
    }

    host.net_session.role = network::NetRole::Host;
    host.net_session.local_player_id = 1;
    host.net_session.host_player_id = 1;
    host.net_session.input_lockstep_enabled = true;
    host.net_session.stage_instance_id = 0x51A7E010U;
    host.net_session.lockstep_input_delay_frames = 0;
    host.net_session.lockstep_max_rollback_frames = network::kDefaultLockstepMaxRollbackFrames;
    network::ResetInputLockstepState(host);
    host.net_transport->remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 9},
        .last_heard_frame = host.frame,
    });
    host.net_session.peers.push_back(network::NetPeerState{
        .player_id = 2,
        .display_name = "Slow Laptop",
        .endpoint_address = "127.0.0.1",
        .endpoint_port = 9,
        .estimated_ping_ms = 300.0F,
        .jitter_ms = 50.0F,
        .connected = true,
    });

    InputFrame slow_initial = InputFrame::New();
    slow_initial.right = true;
    slow_initial.run = true;
    network::LockstepInputRecord slow_record;
    slow_record.player_id = 2;
    slow_record.frame = 0;
    slow_record.sequence = 1;
    slow_record.input = slow_initial;
    host.net_session.lockstep_input_buffer.Store(slow_record);

    InputFrame host_input = InputFrame::New();
    host_input.right = true;
    host_input.run = true;
    host.playing_input_snapshot = ToPlayingInputSnapshot(host_input);

    const auto maintain_and_prepare = [&]() {
        network::MaintainInputLockstepTransport(host, host_graphics);
        return network::PrepareInputLockstepFrame(host, host_graphics);
    };

    if (!maintain_and_prepare()) {
        std::cerr << "host waits for missing input smoke failed: host did not step frame 0\n";
        return false;
    }
    if (host.net_session.lockstep_next_frame_to_step != 1) {
        std::cerr << "host waits for missing input smoke failed: expected frame 1, got "
                  << host.net_session.lockstep_next_frame_to_step << '\n';
        return false;
    }
    StepSingleTickWithMode(
        host,
        host_audio,
        host_graphics,
        SimulationTickMode::ReplayNoNetwork
    );

    if (maintain_and_prepare()) {
        std::cerr << "host waits for missing input smoke failed: host stepped without remote"
                  << " frame 1 input\n";
        return false;
    }
    if (host.net_session.lockstep_next_frame_to_step != 1) {
        std::cerr << "host waits for missing input smoke failed: host advanced while missing"
                  << " remote input, frame=" << host.net_session.lockstep_next_frame_to_step
                  << '\n';
        return false;
    }
    if (host.net_session.lockstep_arbitrated_missing_input_count != 0 ||
        host.net_session.lockstep_arbitrated_neutral_input_count != 0) {
        std::cerr << "host waits for missing input smoke failed: host arbitrated missing input"
                  << ", missing=" << host.net_session.lockstep_arbitrated_missing_input_count
                  << " neutral=" << host.net_session.lockstep_arbitrated_neutral_input_count
                  << '\n';
        return false;
    }

    network::RemoveRemotePlayers(host, *host.net_transport, {2});
    (void)maintain_and_prepare();
    if (!maintain_and_prepare()) {
        std::cerr << "host waits for missing input smoke failed: host did not resume"
                  << " after all remotes disconnected\n";
        return false;
    }

    std::cout << "host waits for missing input smoke ok: frame="
              << host.net_session.lockstep_next_frame_to_step
              << " wait_blocks=" << host.net_session.lockstep_input_wait_block_count
              << " delay=" << host.net_session.lockstep_input_delay_frames << '\n';
    return true;
}

} // namespace

bool CheckJoinBarrierNextStageRestartSmoke() {
    return RunJoinBarrierNextStageRestartSmoke();
}

bool CheckJoinBarrierProtocolSmoke() {
    Graphics graphics;
    InitCliSmokeRuntimeTables(graphics);
    return RunJoinBarrierProtocolSmoke();
}

bool CheckStateFingerprintSmoke() {
    try {
        Graphics graphics;
        InitCliSmokeRuntimeTables(graphics);

        constexpr std::uint32_t seed = 12345;
        State state = State::New();
        if (!LoadQuestStage(state, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "state fingerprint smoke failed: could not load test stage\n";
            return false;
        }

        CanonicalStateFingerprint first_fingerprint = ComputeCanonicalStateFingerprint(state);
        CanonicalStateFingerprint second_fingerprint = ComputeCanonicalStateFingerprint(state);
        if (first_fingerprint.value != second_fingerprint.value) {
            std::cerr << "state fingerprint smoke failed: fingerprint is unstable without mutation\n"
                      << "  first  " << first_fingerprint.summary << " hash="
                      << first_fingerprint.value << "\n"
                      << "  second " << second_fingerprint.summary << " hash="
                      << second_fingerprint.value << "\n";
            return false;
        }

        const IVec2 rope_tile_pos = IVec2::New(2, 2);
        const Ent* source = state.ents.ents.empty()
            ? nullptr
            : &state.ents.ents.front();
        if (source == nullptr) {
            std::cerr << "state fingerprint smoke failed: no source ent\n";
            return false;
        }

        (void)world_ops::PlaceRopeTile(state, *source, rope_tile_pos);

        first_fingerprint = ComputeCanonicalStateFingerprint(state);
        second_fingerprint = ComputeCanonicalStateFingerprint(state);
        if (first_fingerprint.value != second_fingerprint.value) {
            std::cerr << "state fingerprint smoke failed: fingerprint is unstable after world_ops mutation\n"
                      << "  first  " << first_fingerprint.summary << " hash="
                      << first_fingerprint.value << "\n"
                      << "  second " << second_fingerprint.summary << " hash="
                      << second_fingerprint.value << "\n";
            return false;
        }

        State presentation_left = State::New();
        State presentation_right = State::New();
        if (!LoadQuestStage(presentation_left, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(presentation_right, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "state fingerprint smoke failed: could not load presentation test stages\n";
            return false;
        }
        Ent* presentation_ent = nullptr;
        for (Ent& ent : presentation_right.ents.ents) {
            if (ent.active && ent.type_ != EntType::Player) {
                presentation_ent = &ent;
                break;
            }
        }
        if (presentation_ent == nullptr) {
            std::cerr << "state fingerprint smoke failed: no presentation test ent\n";
            return false;
        }
        presentation_ent->render_enabled = !presentation_ent->render_enabled;
        presentation_ent->draw_layer = DrawLayer::Foreground;
        presentation_ent->light_strength += sim::ToSimScalar(0.5F);
        presentation_ent->light_radius += 2;
        presentation_ent->aframe_animator.current_frame += 1;
        presentation_ent->aframe_animator.current_time += sim::ToSimScalar(0.375F);
        presentation_ent->aframe_animator.finished = !presentation_ent->aframe_animator.finished;
        const CanonicalStateFingerprint left_network =
            ComputeNetworkStateFingerprint(presentation_left);
        const CanonicalStateFingerprint right_network =
            ComputeNetworkStateFingerprint(presentation_right);
        if (left_network.value != right_network.value) {
            std::cerr << "state fingerprint smoke failed: network hash included presentation-only ent state\n"
                      << "  left  " << left_network.summary << " hash="
                      << left_network.value << "\n"
                      << "  right " << right_network.summary << " hash="
                      << right_network.value << "\n";
            return false;
        }

        State deactivation_state = State::New();
        if (!LoadQuestStage(deactivation_state, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "state fingerprint smoke failed: could not load deactivation test stage\n";
            return false;
        }
        PlayerSlot* const primary_slot = deactivation_state.players.FindPrimaryLocal();
        if (primary_slot == nullptr || !primary_slot->ent_vid.has_value()) {
            std::cerr << "state fingerprint smoke failed: no deactivation test player\n";
            return false;
        }
        const VID player_vid = *primary_slot->ent_vid;
        FillToolSlot(
            deactivation_state.ent_tools.EnsureToolSlot(player_vid, 0),
            ToolKind::ThrowBomb,
            3,
            true
        );
        if (!world_ops::DeactivateEnt(deactivation_state, player_vid)) {
            std::cerr << "state fingerprint smoke failed: could not deactivate test player\n";
            return false;
        }
        if (primary_slot->ent_vid.has_value() ||
            deactivation_state.ent_tools.FindEntToolState(player_vid) != nullptr) {
            std::cerr << "state fingerprint smoke failed: deactivated player left slot/tool sim state\n";
            return false;
        }

        std::cout << "state fingerprint smoke ok: "
                  << first_fingerprint.summary
                  << " hash=" << first_fingerprint.value << '\n';
        return true;
    } catch (const std::exception& e) {
        std::cerr << "state fingerprint smoke failed: " << e.what() << '\n';
        return false;
    }
}

bool CheckStateEqualitySmoke() {
    try {
        Graphics graphics;
        InitCliSmokeRuntimeTables(graphics);

        constexpr std::uint32_t seed = 12345;
        State left = State::New();
        State right = State::New();
        if (!LoadQuestStage(left, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(right, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "state equality smoke failed: could not load test stages\n";
            return false;
        }

        if (!CompareCanonicalFingerprints(left, right, "after load")) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(left, right) << '\n';
            return false;
        }

        const char* failed_step = nullptr;
        if (!ApplyDetWorldOpsSmokeMutations(left, failed_step)) {
            std::cerr << "state equality smoke failed on left mutation: "
                      << (failed_step != nullptr ? failed_step : "unknown") << '\n';
            return false;
        }
        if (!ApplyDetWorldOpsSmokeMutations(right, failed_step)) {
            std::cerr << "state equality smoke failed on right mutation: "
                      << (failed_step != nullptr ? failed_step : "unknown") << '\n';
            return false;
        }

        if (!CompareCanonicalFingerprints(left, right, "after canonical world_ops mutations")) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(left, right) << '\n';
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "state equality smoke failed: " << e.what() << '\n';
        return false;
    }
}

// Regression: recording/snapshot restore must re-derive runtime callbacks from
// the build-local type spec. Serialized snapshots omit function-pointer bytes,
// and in-memory snapshots may still carry null/stale callback values after tests
// or older tools mutate them.
bool CheckGameplaySnapshotCallbackRebindSmoke() {
    try {
        Graphics graphics;
        InitCliSmokeRuntimeTables(graphics);

        State state = State::New();
        if (!LoadQuestStage(state, "classic", "classic_mines_1", false, 12345)) {
            std::cerr << "snapshot callback rebind smoke failed: could not load stage\n";
            return false;
        }

        GameplaySnapshot snapshot = MakeGameplaySnapshot(state, graphics);

        // Simulate stale/garbage pointers from a loaded recording.
        for (Ent& ent : snapshot.ents.ents) {
            ent.on_death = nullptr;
            ent.on_damage = nullptr;
            ent.on_use = nullptr;
            ent.on_area_enter = nullptr;
            ent.on_area_exit = nullptr;
            ent.on_area_tile_changed = nullptr;
            ent.control_logic = nullptr;
            ent.step_logic = nullptr;
            ent.step_physics = nullptr;
        }

        RestoreGameplaySnapshot(snapshot, state, graphics);

        std::size_t checked_with_callbacks = 0;
        for (const Ent& ent : state.ents.ents) {
            if (!ent.active) {
                continue;
            }
            const EntSpec& spec = GetEntSpec(ent.type_);
            const bool spec_has_callback =
                spec.on_death != nullptr || spec.on_damage != nullptr ||
                spec.on_use != nullptr || spec.on_area_enter != nullptr ||
                spec.on_area_exit != nullptr || spec.on_area_tile_changed != nullptr ||
                spec.control_logic != nullptr || spec.step_logic != nullptr ||
                spec.step_physics != nullptr;
            if (spec_has_callback) {
                ++checked_with_callbacks;
            }
            if (ent.on_death != spec.on_death || ent.on_damage != spec.on_damage ||
                ent.on_use != spec.on_use || ent.on_area_enter != spec.on_area_enter ||
                ent.on_area_exit != spec.on_area_exit ||
                ent.on_area_tile_changed != spec.on_area_tile_changed ||
                ent.control_logic != spec.control_logic ||
                ent.step_logic != spec.step_logic ||
                ent.step_physics != spec.step_physics) {
                std::cerr << "snapshot callback rebind smoke failed: ent id "
                          << ent.vid.id << " callbacks not rebound from spec\n";
                return false;
            }
        }

        if (checked_with_callbacks == 0) {
            std::cerr << "snapshot callback rebind smoke failed: no active ents "
                         "with spec callbacks to verify (test would be vacuous)\n";
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "snapshot callback rebind smoke failed: " << e.what() << '\n';
        return false;
    }
}

bool CheckDetReplaySmoke() {
    try {
        Graphics graphics;
        InitCliSmokeRuntimeTables(graphics);
        Audio audio;

        constexpr std::uint32_t seed = 12345;
        State recorded = State::New();
        State replayed = State::New();
        if (!LoadQuestStage(recorded, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(replayed, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "det replay smoke failed: could not load test stages\n";
            return false;
        }

        if (!CompareCanonicalFingerprints(recorded, replayed, "det replay initial")) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(recorded, replayed) << '\n';
            return false;
        }

        const std::vector<InputFrame> inputs = BuildDetReplayInputScript();
        for (std::size_t frame_index = 0; frame_index < inputs.size(); ++frame_index) {
            ApplyPrimaryInputFrame(recorded, inputs[frame_index]);
            ApplyPrimaryInputFrame(replayed, inputs[frame_index]);
            StepSingleTick(recorded, audio, graphics);
            StepSingleTick(replayed, audio, graphics);

            const CanonicalStateFingerprint recorded_fingerprint =
                ComputeGameplayDeterminismFingerprint(recorded);
            const CanonicalStateFingerprint replayed_fingerprint =
                ComputeGameplayDeterminismFingerprint(replayed);
            if (recorded_fingerprint.value != replayed_fingerprint.value) {
                std::cerr << "det replay smoke failed at frame "
                          << frame_index << "\n"
                          << "  recorded " << recorded_fingerprint.summary
                          << " hash=" << recorded_fingerprint.value << "\n"
                          << "  replayed " << replayed_fingerprint.summary
                          << " hash=" << replayed_fingerprint.value << "\n"
                          << "  first simple diff: "
                          << DescribeFirstStateDifference(recorded, replayed) << '\n';
                return false;
            }
        }

        const CanonicalStateFingerprint final_fingerprint =
            ComputeGameplayDeterminismFingerprint(recorded);
        std::cout << "det replay smoke ok: frames=" << inputs.size()
                  << " " << final_fingerprint.summary
                  << " hash=" << final_fingerprint.value << '\n';

        State multi_recorded = State::New();
        State multi_replayed = State::New();
        if (!LoadQuestStage(multi_recorded, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(multi_replayed, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "det multi-local replay smoke failed: could not load test stages\n";
            return false;
        }
        if (!AddSecondLocalPlayerForDetReplay(multi_recorded, graphics) ||
            !AddSecondLocalPlayerForDetReplay(multi_replayed, graphics)) {
            std::cerr << "det multi-local replay smoke failed: could not spawn second player\n";
            return false;
        }
        if (!CompareCanonicalFingerprints(
                multi_recorded,
                multi_replayed,
                "det multi-local replay initial"
            )) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(multi_recorded, multi_replayed) << '\n';
            return false;
        }

        const std::vector<std::array<InputFrame, 2>> multi_inputs =
            BuildDetMultiLocalReplayInputScript();
        for (std::size_t frame_index = 0; frame_index < multi_inputs.size(); ++frame_index) {
            ApplyMultiLocalInputFrame(multi_recorded, multi_inputs[frame_index]);
            ApplyMultiLocalInputFrame(multi_replayed, multi_inputs[frame_index]);
            StepSingleTick(multi_recorded, audio, graphics);
            StepSingleTick(multi_replayed, audio, graphics);

            const CanonicalStateFingerprint recorded_fingerprint =
                ComputeGameplayDeterminismFingerprint(multi_recorded);
            const CanonicalStateFingerprint replayed_fingerprint =
                ComputeGameplayDeterminismFingerprint(multi_replayed);
            if (recorded_fingerprint.value != replayed_fingerprint.value) {
                std::cerr << "det multi-local replay smoke failed at frame "
                          << frame_index << "\n"
                          << "  recorded " << recorded_fingerprint.summary
                          << " hash=" << recorded_fingerprint.value << "\n"
                          << "  replayed " << replayed_fingerprint.summary
                          << " hash=" << replayed_fingerprint.value << "\n"
                          << "  first simple diff: "
                          << DescribeFirstStateDifference(multi_recorded, multi_replayed) << '\n';
                return false;
            }
        }

        const CanonicalStateFingerprint multi_final_fingerprint =
            ComputeGameplayDeterminismFingerprint(multi_recorded);
        std::cout << "det multi-local replay smoke ok: frames="
                  << multi_inputs.size() << " " << multi_final_fingerprint.summary
                  << " hash=" << multi_final_fingerprint.value << '\n';

        State broad_recorded = State::New();
        State broad_replayed = State::New();
        if (!LoadQuestStage(broad_recorded, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(broad_replayed, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "det broad replay smoke failed: could not load test stages\n";
            return false;
        }
        const char* failed_step = nullptr;
        if (!PrepareBroadDetReplayScenario(broad_recorded, failed_step) ||
            !PrepareBroadDetReplayScenario(broad_replayed, failed_step)) {
            std::cerr << "det broad replay smoke failed during setup: "
                      << (failed_step != nullptr ? failed_step : "unknown") << '\n';
            return false;
        }
        if (!CompareCanonicalFingerprints(
                broad_recorded,
                broad_replayed,
                "det broad replay initial"
            )) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(broad_recorded, broad_replayed) << '\n';
            return false;
        }

        const std::vector<InputFrame> broad_inputs =
            BuildBroadDetReplayInputScript();
        for (std::size_t frame_index = 0; frame_index < broad_inputs.size(); ++frame_index) {
            ApplyPrimaryInputFrame(broad_recorded, broad_inputs[frame_index]);
            ApplyPrimaryInputFrame(broad_replayed, broad_inputs[frame_index]);
            StepSingleTick(broad_recorded, audio, graphics);
            StepSingleTick(broad_replayed, audio, graphics);

            const CanonicalStateFingerprint recorded_fingerprint =
                ComputeGameplayDeterminismFingerprint(broad_recorded);
            const CanonicalStateFingerprint replayed_fingerprint =
                ComputeGameplayDeterminismFingerprint(broad_replayed);
            if (recorded_fingerprint.value != replayed_fingerprint.value) {
                std::cerr << "det broad replay smoke failed at frame "
                          << frame_index << "\n"
                          << "  recorded " << recorded_fingerprint.summary
                          << " hash=" << recorded_fingerprint.value << "\n"
                          << "  replayed " << replayed_fingerprint.summary
                          << " hash=" << replayed_fingerprint.value << "\n"
                          << "  first simple diff: "
                          << DescribeFirstStateDifference(broad_recorded, broad_replayed) << '\n';
                return false;
            }
        }

        const CanonicalStateFingerprint broad_final_fingerprint =
            ComputeGameplayDeterminismFingerprint(broad_recorded);
        std::cout << "det broad replay smoke ok: frames="
                  << broad_inputs.size() << " " << broad_final_fingerprint.summary
                  << " hash=" << broad_final_fingerprint.value << '\n';

        State fluid_recorded = State::New();
        State fluid_replayed = State::New();
        if (!LoadQuestStage(fluid_recorded, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(fluid_replayed, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "det fluid replay smoke failed: could not load test stages\n";
            return false;
        }
        failed_step = nullptr;
        if (!PrepareFluidDetReplayScenario(fluid_recorded, failed_step) ||
            !PrepareFluidDetReplayScenario(fluid_replayed, failed_step)) {
            std::cerr << "det fluid replay smoke failed during setup: "
                      << (failed_step != nullptr ? failed_step : "unknown") << '\n';
            return false;
        }
        if (!RunSinglePlayerDetReplayScenario(
                fluid_recorded,
                fluid_replayed,
                audio,
                graphics,
                BuildNeutralInputScript(240),
                "det fluid replay smoke"
            )) {
            return false;
        }

        State shop_recorded = State::New();
        State shop_replayed = State::New();
        failed_step = nullptr;
        if (!PrepareShopDetReplayScenario(shop_recorded, failed_step) ||
            !PrepareShopDetReplayScenario(shop_replayed, failed_step)) {
            std::cerr << "det shop replay smoke failed during setup: "
                      << (failed_step != nullptr ? failed_step : "unknown") << '\n';
            return false;
        }
        if (!RunSinglePlayerDetReplayScenario(
                shop_recorded,
                shop_replayed,
                audio,
                graphics,
                BuildShopDetReplayInputScript(),
                "det shop replay smoke"
            )) {
            return false;
        }

        State transition_recorded = State::New();
        State transition_replayed = State::New();
        if (!LoadQuestStage(transition_recorded, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(transition_replayed, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "det stage-transition replay smoke failed: could not load test stages\n";
            return false;
        }
        if (!CompareCanonicalFingerprints(
                transition_recorded,
                transition_replayed,
                "det stage-transition replay initial"
            )) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(transition_recorded, transition_replayed) << '\n';
            return false;
        }
        const StageTransitionTarget transition{
            .destination = StageLoadTarget::ForQuestStage("classic", "classic_mines_2"),
            .preserve_player_state = true,
            .seed = seed,
        };
        QueueStageTransition(transition_recorded, transition);
        QueueStageTransition(transition_replayed, transition);
        ApplyPendingStageTransition(transition_recorded);
        ApplyPendingStageTransition(transition_replayed);
        transition_recorded.SetMode(Mode::Playing);
        transition_replayed.SetMode(Mode::Playing);
        for (std::size_t frame_index = 0; frame_index < 70; ++frame_index) {
            StepSingleTick(transition_recorded, audio, graphics);
            StepSingleTick(transition_replayed, audio, graphics);
            if (!CompareDetReplayFingerprints(
                    transition_recorded,
                    transition_replayed,
                    "det stage-transition replay smoke",
                    frame_index
                )) {
                return false;
            }
        }
        if (transition_recorded.stage.quest_stage_id != "classic_mines_2" ||
            transition_replayed.stage.quest_stage_id != "classic_mines_2") {
            std::cerr << "det stage-transition replay smoke failed: did not enter classic_mines_2\n";
            return false;
        }
        const CanonicalStateFingerprint transition_final_fingerprint =
            ComputeGameplayDeterminismFingerprint(transition_recorded);
        std::cout << "det stage-transition replay smoke ok: frames=70 "
                  << transition_final_fingerprint.summary
                  << " hash=" << transition_final_fingerprint.value << '\n';
        return true;
    } catch (const std::exception& e) {
        std::cerr << "det replay smoke failed: " << e.what() << '\n';
        return false;
    }
}

bool CheckNetworkFreshReloadOwnershipSmoke() {
    try {
        Graphics host_graphics;
        Graphics peer_graphics;
        InitCliSmokeRuntimeTables(host_graphics);
        InitCliSmokeRuntimeTables(peer_graphics);
        Audio host_audio;
        Audio peer_audio;
        FakeLockstepPeer host;
        FakeLockstepPeer peer;
        host.owned_players = {1};
        peer.owned_players = {2};

        const char* failed_step = nullptr;
        if (!PrepareLockstepSmokeState(host.state, host_graphics, host.owned_players, failed_step) ||
            !PrepareLockstepSmokeState(peer.state, peer_graphics, peer.owned_players, failed_step)) {
            std::cerr << "network fresh reload ownership smoke failed during setup: "
                      << (failed_step != nullptr ? failed_step : "unknown") << '\n';
            return false;
        }
        ConfigureSmokeNetworkRoles(host.state, peer.state);
        if (!RunNetworkFreshQuestReloadSmoke(
                host.state,
                peer.state,
                host_audio,
                peer_audio,
                host_graphics,
                peer_graphics,
                87654U
            ) ||
            !SmokePlayerIsAlive(host.state, 1) ||
            !SmokePlayerIsAlive(host.state, 2) ||
            !SmokePlayerIsAlive(peer.state, 1) ||
            !SmokePlayerIsAlive(peer.state, 2) ||
            !SmokePlayerOwnershipMatches(host.state, 1, PlayerConnectionKind::Local, true) ||
            !SmokePlayerOwnershipMatches(host.state, 2, PlayerConnectionKind::Remote, false) ||
            !SmokePlayerOwnershipMatches(peer.state, 1, PlayerConnectionKind::Remote, false) ||
            !SmokePlayerOwnershipMatches(peer.state, 2, PlayerConnectionKind::Local, true)) {
            std::cerr << "network fresh reload ownership smoke failed\n"
                      << "  host ownership:" << DescribeSmokePlayerOwnership(host.state) << '\n'
                      << "  peer ownership:" << DescribeSmokePlayerOwnership(peer.state) << '\n';
            return false;
        }

        std::cout << "network fresh reload ownership smoke ok\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "network fresh reload ownership smoke failed: " << e.what() << '\n';
        return false;
    }
}

bool CheckInputLockstepSmoke() {
    try {
        Graphics peer0_graphics;
        Graphics peer1_graphics;
        InitCliSmokeRuntimeTables(peer0_graphics);
        InitCliSmokeRuntimeTables(peer1_graphics);
        Audio peer0_audio;
        Audio peer1_audio;

        if (!RunCanonicalInputBufferSmoke()) {
            return false;
        }

        const std::vector<std::array<InputFrame, 2>> script =
            BuildInputLockstepSmokeScript();
        const std::vector<PlayerId> required_players = {1, 2};

        const auto run_case = [&](
            const char* label,
            const network::NetFuzzerConfig& fuzzer,
            std::uint32_t fuzzer_seed,
            network::LockstepFrame input_delay_frames,
            FakeLockstepRunRateSchedule run_rate = {}
        ) -> bool {
            const network::LockstepFrame total_frames =
                static_cast<network::LockstepFrame>(script.size());
            constexpr std::uint64_t kMaxWallTicks = 6000;

            FakeLockstepPeer peer0;
            peer0.peer_id = 0;
            peer0.owned_players = {1};
            FakeLockstepPeer peer1;
            peer1.peer_id = 1;
            peer1.owned_players = {2};

            const char* failed_step = nullptr;
            if (!PrepareLockstepSmokeState(peer0.state, peer0_graphics, peer0.owned_players, failed_step) ||
                !PrepareLockstepSmokeState(peer1.state, peer1_graphics, peer1.owned_players, failed_step)) {
                std::cerr << "input lockstep smoke " << label
                          << " failed during setup: "
                          << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                return false;
            }
            peer0.state.net_session.lockstep_input_delay_frames =
                network::ClampLockstepInputDelayFrames(static_cast<std::uint32_t>(input_delay_frames));
            peer1.state.net_session.lockstep_input_delay_frames =
                network::ClampLockstepInputDelayFrames(static_cast<std::uint32_t>(input_delay_frames));

            if (!CompareCanonicalFingerprints(peer0.state, peer1.state, "input lockstep initial")) {
                std::cerr << "  first simple diff: "
                          << DescribeFirstStateDifference(peer0.state, peer1.state) << '\n';
                return false;
            }

            FakeLockstepNetwork network = FakeLockstepNetwork::New(fuzzer, fuzzer_seed);
            std::size_t compared_hashes = 0;
            std::string step_error;

            for (std::uint64_t wall_tick = 0; wall_tick < kMaxWallTicks; ++wall_tick) {
                const network::LockstepFrame latest_input_frame =
                    std::min<network::LockstepFrame>(
                        static_cast<network::LockstepFrame>(wall_tick) + input_delay_frames,
                        total_frames - 1
                    );

                const bool pump_peer0 =
                    ShouldPumpFakePeer(run_rate, peer0.peer_id, wall_tick);
                const bool pump_peer1 =
                    ShouldPumpFakePeer(run_rate, peer1.peer_id, wall_tick);
                if (pump_peer0) {
                    const network::LockstepInputPacket p0_packet =
                        BuildLockstepInputPacket(peer0, script, latest_input_frame);
                    network.Send(wall_tick, peer0.peer_id, peer1.peer_id, p0_packet);
                    for (const network::LockstepInputPacket& packet :
                         network.ReceiveForPeer(wall_tick, peer0.peer_id)) {
                        for (const network::LockstepInputRecord& record : packet.records) {
                            peer0.input_buffer.Store(record);
                        }
                    }
                    if (!StepReadyLockstepFrames(
                            peer0,
                            required_players,
                            peer0_audio,
                            peer0_graphics,
                            total_frames,
                            step_error
                        )) {
                        std::cerr << "input lockstep smoke " << label
                                  << " failed while stepping peer0: " << step_error << '\n';
                        return false;
                    }
                }
                if (pump_peer1) {
                    const network::LockstepInputPacket p1_packet =
                        BuildLockstepInputPacket(peer1, script, latest_input_frame);
                    network.Send(wall_tick, peer1.peer_id, peer0.peer_id, p1_packet);
                    for (const network::LockstepInputPacket& packet :
                         network.ReceiveForPeer(wall_tick, peer1.peer_id)) {
                        for (const network::LockstepInputRecord& record : packet.records) {
                            peer1.input_buffer.Store(record);
                        }
                    }
                    if (!StepReadyLockstepFrames(
                            peer1,
                            required_players,
                            peer1_audio,
                            peer1_graphics,
                            total_frames,
                            step_error
                        )) {
                        std::cerr << "input lockstep smoke " << label
                                  << " failed while stepping peer1: " << step_error << '\n';
                        return false;
                    }
                }

                const std::size_t comparable_hashes =
                    std::min(peer0.frame_hashes.size(), peer1.frame_hashes.size());
                while (compared_hashes < comparable_hashes) {
                    if (peer0.frame_hashes[compared_hashes] !=
                        peer1.frame_hashes[compared_hashes]) {
                        std::cerr << "input lockstep smoke " << label
                                  << " hash mismatch at frame " << compared_hashes
                                  << "\n  peer0 hash=" << peer0.frame_hashes[compared_hashes]
                                  << "\n  peer1 hash=" << peer1.frame_hashes[compared_hashes]
                                  << "\n  first simple diff: "
                                  << DescribeFirstStateDifference(peer0.state, peer1.state)
                                  << '\n';
                        return false;
                    }
                    compared_hashes += 1;
                }

                if (peer0.next_frame_to_step >= total_frames &&
                    peer1.next_frame_to_step >= total_frames) {
                    const CanonicalStateFingerprint final_fingerprint =
                        ComputeGameplayDeterminismFingerprint(peer0.state);
                    std::cout << "input lockstep smoke " << label
                              << " ok: frames=" << total_frames
                              << " input_delay=" << input_delay_frames
                              << " wall_ticks=" << (wall_tick + 1)
                              << " sent=" << network.stats.packets_sent
                              << " recv=" << network.stats.packets_received
                              << " drop=" << network.stats.packets_dropped
                              << " dup=" << network.stats.packets_duplicated
                              << " " << final_fingerprint.summary
                              << " hash=" << final_fingerprint.value << '\n';
                    return true;
                }
            }

            std::cerr << "input lockstep smoke " << label
                      << " timed out:"
                      << " peer0_frame=" << peer0.next_frame_to_step
                      << " peer1_frame=" << peer1.next_frame_to_step
                      << " sent=" << network.stats.packets_sent
                      << " recv=" << network.stats.packets_received
                      << " drop=" << network.stats.packets_dropped
                      << " dup=" << network.stats.packets_duplicated << '\n';
            return false;
        };

        network::NetFuzzerConfig clean;
        clean.enabled = false;
        if (!run_case("clean", clean, 0x1001U, 3)) {
            return false;
        }

        network::NetFuzzerConfig impaired = network::NetFuzzerConfig::TexasToCaliforniaPreset();
        impaired.duplicate_percent = 4.0F;
        impaired.reorder_window_packets = 4;
        if (!run_case("impaired", impaired, 0x1002U, network::kDefaultLockstepInputDelayFrames)) {
            return false;
        }
        struct FuzzerProfileSmokeCase {
            const char* label = "";
            network::NetFuzzerConfig config{};
            std::uint32_t seed = 0;
            network::LockstepFrame input_delay_frames = network::kDefaultLockstepInputDelayFrames;
        };
        const std::array<FuzzerProfileSmokeCase, 7> profile_cases = {{
            FuzzerProfileSmokeCase{
                .label = "same-house-profile",
                .config = network::NetFuzzerConfig::SameHousePreset(),
                .seed = 0x1101U,
                .input_delay_frames = 2,
            },
            FuzzerProfileSmokeCase{
                .label = "same-city-profile",
                .config = network::NetFuzzerConfig::SameCityPreset(),
                .seed = 0x1102U,
                .input_delay_frames = 3,
            },
            FuzzerProfileSmokeCase{
                .label = "same-state-profile",
                .config = network::NetFuzzerConfig::SameStatePreset(),
                .seed = 0x1103U,
                .input_delay_frames = 4,
            },
            FuzzerProfileSmokeCase{
                .label = "tx-ca-profile",
                .config = network::NetFuzzerConfig::TexasToCaliforniaPreset(),
                .seed = 0x1104U,
                .input_delay_frames = 5,
            },
            FuzzerProfileSmokeCase{
                .label = "ca-fl-profile",
                .config = network::NetFuzzerConfig::CaliforniaToFloridaPreset(),
                .seed = 0x1105U,
                .input_delay_frames = 6,
            },
            FuzzerProfileSmokeCase{
                .label = "us-cross-country-profile",
                .config = network::NetFuzzerConfig::UsCrossCountryPreset(),
                .seed = 0x1106U,
                .input_delay_frames = 7,
            },
            FuzzerProfileSmokeCase{
                .label = "tx-japan-profile",
                .config = network::NetFuzzerConfig::JapanToTexasPreset(),
                .seed = 0x1107U,
                .input_delay_frames = 8,
            },
        }};
        for (const FuzzerProfileSmokeCase& profile_case : profile_cases) {
            if (!run_case(
                    profile_case.label,
                    profile_case.config,
                    profile_case.seed,
                    profile_case.input_delay_frames
                )) {
                return false;
            }
        }
        FakeLockstepRunRateSchedule run_rate_skew;
        run_rate_skew.peer1_pump_every_ticks = 2;
        run_rate_skew.peer0_hitch_every_ticks = 97;
        run_rate_skew.peer0_hitch_length_ticks = 3;
        run_rate_skew.peer1_hitch_every_ticks = 131;
        run_rate_skew.peer1_hitch_length_ticks = 5;
        if (!run_case("run-rate-skew", clean, 0x1003U, 8, run_rate_skew)) {
            return false;
        }
        if (!RunLockstepSettingsScheduleSmoke()) {
            return false;
        }
        if (!RunHostWaitsForMissingInputSmoke()) {
            return false;
        }
        if (!RunJoinBarrierProtocolSmoke()) {
            return false;
        }
        if (!RunJoinBarrierChunkImpairmentSmoke()) {
            return false;
        }
        if (!RunJoinBarrierNextStageRestartSmoke()) {
            return false;
        }
        if (!RunSimSnapshotMultiLocalOverlaySmoke()) {
            return false;
        }
        if (!RunRetainedReconnectSmoke()) {
            return false;
        }

        const auto run_carry_transition_case = [&]() -> bool {
            const std::vector<std::array<InputFrame, 2>> carry_script =
                BuildInputLockstepCarryScript();
            const network::LockstepFrame total_frames =
                static_cast<network::LockstepFrame>(carry_script.size());
            constexpr network::LockstepFrame kInputDelayFrames = 8;
            constexpr std::uint64_t kMaxWallTicks = 2000;

            FakeLockstepPeer peer0;
            peer0.peer_id = 0;
            peer0.owned_players = {1};
            FakeLockstepPeer peer1;
            peer1.peer_id = 1;
            peer1.owned_players = {2};

            const char* failed_step = nullptr;
            if (!PrepareLockstepSmokeState(peer0.state, peer0_graphics, peer0.owned_players, failed_step) ||
                !PrepareLockstepSmokeState(peer1.state, peer1_graphics, peer1.owned_players, failed_step)) {
                std::cerr << "input lockstep carry-transition smoke failed during setup: "
                          << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                return false;
            }
            if (!PlaceCarryTransitionSmokePlayers(peer0.state, peer0_graphics, failed_step) ||
                !PlaceCarryTransitionSmokePlayers(peer1.state, peer1_graphics, failed_step)) {
                std::cerr << "input lockstep carry-transition smoke failed during player placement: "
                          << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                return false;
            }

            const std::optional<VID> peer0_p1 = FindPlayerVidForSmoke(peer0.state, 1);
            const std::optional<VID> peer0_p2 = FindPlayerVidForSmoke(peer0.state, 2);
            const std::optional<VID> peer1_p1 = FindPlayerVidForSmoke(peer1.state, 1);
            const std::optional<VID> peer1_p2 = FindPlayerVidForSmoke(peer1.state, 2);
            if (!peer0_p1.has_value() || !peer0_p2.has_value() ||
                !peer1_p1.has_value() || !peer1_p2.has_value()) {
                std::cerr << "input lockstep carry-transition smoke failed: missing players after placement\n";
                return false;
            }

            if (!ents::common::TryPickupEntByVid(*peer0_p2, *peer0_p1, peer0.state, peer0_graphics) ||
                !ents::common::TryPickupEntByVid(*peer1_p2, *peer1_p1, peer1.state, peer1_graphics)) {
                std::cerr << "input lockstep carry-transition smoke failed: peer-owned player could not carry host-owned player\n";
                return false;
            }
            if (!CompareCanonicalFingerprints(
                    peer0.state,
                    peer1.state,
                    "input lockstep carry setup"
                )) {
                std::cerr << "  first simple diff: "
                          << DescribeFirstStateDifference(peer0.state, peer1.state) << '\n';
                return false;
            }

            if (!ents::common::TryDropEntByVid(*peer0_p2, *peer0_p1, peer0.state, peer0_graphics) ||
                !ents::common::TryDropEntByVid(*peer1_p2, *peer1_p1, peer1.state, peer1_graphics)) {
                std::cerr << "input lockstep carry-transition smoke failed: peer-owned player could not drop host-owned player\n";
                return false;
            }
            if (!ValidateNoPlayerCarryLinks(peer0.state, "carry-transition after drop", std::cerr) ||
                !ValidateNoPlayerCarryLinks(peer1.state, "carry-transition after drop", std::cerr) ||
                !CompareCanonicalFingerprints(peer0.state, peer1.state, "input lockstep carry drop")) {
                std::cerr << "  first simple diff: "
                          << DescribeFirstStateDifference(peer0.state, peer1.state) << '\n';
                return false;
            }

            if (!ents::common::TryPickupEntByVid(*peer0_p2, *peer0_p1, peer0.state, peer0_graphics) ||
                !ents::common::TryPickupEntByVid(*peer1_p2, *peer1_p1, peer1.state, peer1_graphics)) {
                std::cerr << "input lockstep carry-transition smoke failed: peer-owned player could not re-pickup host-owned player\n";
                return false;
            }
            const Vec2 throw_velocity = Vec2::New(2.0F, -2.0F);
            if (!ents::common::TryThrowEntByVid(
                    *peer0_p2,
                    *peer0_p1,
                    throw_velocity,
                    peer0.state,
                    peer0_graphics,
                    peer0_audio
                ) ||
                !ents::common::TryThrowEntByVid(
                    *peer1_p2,
                    *peer1_p1,
                    throw_velocity,
                    peer1.state,
                    peer1_graphics,
                    peer1_audio
                )) {
                std::cerr << "input lockstep carry-transition smoke failed: peer-owned player could not throw host-owned player\n";
                return false;
            }
            if (!ValidateNoPlayerCarryLinks(peer0.state, "carry-transition after throw", std::cerr) ||
                !ValidateNoPlayerCarryLinks(peer1.state, "carry-transition after throw", std::cerr) ||
                !CompareCanonicalFingerprints(peer0.state, peer1.state, "input lockstep carry throw")) {
                std::cerr << "  first simple diff: "
                          << DescribeFirstStateDifference(peer0.state, peer1.state) << '\n';
                return false;
            }

            if (!PlaceCarryTransitionSmokePlayers(peer0.state, peer0_graphics, failed_step) ||
                !PlaceCarryTransitionSmokePlayers(peer1.state, peer1_graphics, failed_step) ||
                !ents::common::TryPickupEntByVid(*peer0_p2, *peer0_p1, peer0.state, peer0_graphics) ||
                !ents::common::TryPickupEntByVid(*peer1_p2, *peer1_p1, peer1.state, peer1_graphics)) {
                std::cerr << "input lockstep carry-transition smoke failed: could not reset carry setup after drop/throw coverage\n";
                return false;
            }
            if (!CompareCanonicalFingerprints(
                    peer0.state,
                    peer1.state,
                    "input lockstep carry reset after throw"
                )) {
                std::cerr << "  first simple diff: "
                          << DescribeFirstStateDifference(peer0.state, peer1.state) << '\n';
                return false;
            }

            FakeLockstepNetwork network = FakeLockstepNetwork::New(network::NetFuzzerConfig{}, 0x2001U);
            std::size_t compared_hashes = 0;
            std::string step_error;

            for (std::uint64_t wall_tick = 0; wall_tick < kMaxWallTicks; ++wall_tick) {
                const network::LockstepFrame latest_input_frame =
                    std::min<network::LockstepFrame>(
                        static_cast<network::LockstepFrame>(wall_tick) + kInputDelayFrames,
                        total_frames - 1
                    );

                const network::LockstepInputPacket p0_packet =
                    BuildLockstepInputPacket(peer0, carry_script, latest_input_frame);
                const network::LockstepInputPacket p1_packet =
                    BuildLockstepInputPacket(peer1, carry_script, latest_input_frame);
                network.Send(wall_tick, peer0.peer_id, peer1.peer_id, p0_packet);
                network.Send(wall_tick, peer1.peer_id, peer0.peer_id, p1_packet);

                for (const network::LockstepInputPacket& packet :
                     network.ReceiveForPeer(wall_tick, peer0.peer_id)) {
                    for (const network::LockstepInputRecord& record : packet.records) {
                        peer0.input_buffer.Store(record);
                    }
                }
                for (const network::LockstepInputPacket& packet :
                     network.ReceiveForPeer(wall_tick, peer1.peer_id)) {
                    for (const network::LockstepInputRecord& record : packet.records) {
                        peer1.input_buffer.Store(record);
                    }
                }

                if (!StepReadyLockstepFrames(
                        peer0,
                        required_players,
                        peer0_audio,
                        peer0_graphics,
                        total_frames,
                        step_error
                    ) ||
                    !StepReadyLockstepFrames(
                        peer1,
                        required_players,
                        peer1_audio,
                        peer1_graphics,
                        total_frames,
                        step_error
                    )) {
                    std::cerr << "input lockstep carry-transition smoke failed while stepping: "
                              << step_error << '\n';
                    return false;
                }

                const std::size_t comparable_hashes =
                    std::min(peer0.frame_hashes.size(), peer1.frame_hashes.size());
                while (compared_hashes < comparable_hashes) {
                    if (peer0.frame_hashes[compared_hashes] !=
                        peer1.frame_hashes[compared_hashes]) {
                        std::cerr << "input lockstep carry-transition smoke hash mismatch at frame "
                                  << compared_hashes
                                  << "\n  peer0 hash=" << peer0.frame_hashes[compared_hashes]
                                  << "\n  peer1 hash=" << peer1.frame_hashes[compared_hashes]
                                  << "\n  first simple diff: "
                                  << DescribeFirstStateDifference(peer0.state, peer1.state)
                                  << '\n';
                        return false;
                    }
                    compared_hashes += 1;
                }

                if (peer0.next_frame_to_step >= total_frames &&
                    peer1.next_frame_to_step >= total_frames) {
                    break;
                }
            }

            if (peer0.next_frame_to_step < total_frames ||
                peer1.next_frame_to_step < total_frames) {
                std::cerr << "input lockstep carry-transition smoke timed out:"
                          << " peer0_frame=" << peer0.next_frame_to_step
                          << " peer1_frame=" << peer1.next_frame_to_step << '\n';
                return false;
            }

            if (!ValidateActiveSmokePlayers(peer0.state, "carry-transition pre-transition", std::cerr) ||
                !ValidateActiveSmokePlayers(peer1.state, "carry-transition pre-transition", std::cerr)) {
                return false;
            }

            const StageTransitionTarget transition{
                .destination = StageLoadTarget::ForQuestStage("classic", "classic_mines_2"),
                .preserve_player_state = true,
                .seed = 54321U,
            };
            peer0.state.net_session.role = network::NetRole::Host;
            peer1.state.net_session.role = network::NetRole::Peer;
            QueueStageTransition(peer0.state, transition);
            QueueStageTransition(peer1.state, transition);
            peer0.state.SetMode(Mode::StageTransition);
            peer1.state.SetMode(Mode::StageTransition);
            peer0.state.scene_frame = 0;
            peer1.state.scene_frame = 0;
            for (std::uint32_t i = 0; i < 70; ++i) {
                StepSingleTick(peer0.state, peer0_audio, peer0_graphics);
                StepSingleTick(peer1.state, peer1_audio, peer1_graphics);
            }

            if (peer0.state.stage.quest_stage_id != "classic_mines_2" ||
                peer1.state.stage.quest_stage_id != "classic_mines_2") {
                std::cerr << "input lockstep carry-transition smoke failed: did not enter classic_mines_2\n";
                return false;
            }
            if (!ValidateActiveSmokePlayers(peer0.state, "carry-transition post-transition", std::cerr) ||
                !ValidateActiveSmokePlayers(peer1.state, "carry-transition post-transition", std::cerr) ||
                !ValidateNoPlayerCarryLinks(peer0.state, "carry-transition post-transition", std::cerr) ||
                !ValidateNoPlayerCarryLinks(peer1.state, "carry-transition post-transition", std::cerr)) {
                return false;
            }
            if (!CompareCanonicalFingerprints(
                    peer0.state,
                    peer1.state,
                    "input lockstep carry transition final"
                )) {
                std::cerr << "  first simple diff: "
                          << DescribeFirstStateDifference(peer0.state, peer1.state) << '\n';
                return false;
            }

            const CanonicalStateFingerprint final_fingerprint =
                ComputeGameplayDeterminismFingerprint(peer0.state);
            std::cout << "input lockstep carry-transition smoke ok: frames="
                      << total_frames
                      << " " << final_fingerprint.summary
                      << " hash=" << final_fingerprint.value << '\n';
            return true;
        };

        if (!run_carry_transition_case()) {
            return false;
        }

        const auto run_respawn_policy_case = [&]() -> bool {
            const auto prepare_pair = [&](
                FakeLockstepPeer& peer0,
                FakeLockstepPeer& peer1,
                MultiplayerRespawnMode mode
            ) -> bool {
                peer0 = FakeLockstepPeer{};
                peer1 = FakeLockstepPeer{};
                peer0.peer_id = 0;
                peer0.owned_players = {1};
                peer1.peer_id = 1;
                peer1.owned_players = {2};

                const char* failed_step = nullptr;
                if (!PrepareLockstepSmokeState(peer0.state, peer0_graphics, peer0.owned_players, failed_step) ||
                    !PrepareLockstepSmokeState(peer1.state, peer1_graphics, peer1.owned_players, failed_step)) {
                    std::cerr << "input lockstep respawn-policy smoke failed during setup: "
                              << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                    return false;
                }
                if (!PrepareRespawnPolicySmokeEntrance(peer0.state, failed_step) ||
                    !PrepareRespawnPolicySmokeEntrance(peer1.state, failed_step)) {
                    std::cerr << "input lockstep respawn-policy smoke failed during entrance setup: "
                              << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                    return false;
                }
                ConfigureSmokeNetworkRoles(peer0.state, peer1.state);
                peer0.state.multiplayer_respawn_mode = mode;
                peer1.state.multiplayer_respawn_mode = mode;
                return true;
            };

            FakeLockstepPeer peer0;
            FakeLockstepPeer peer1;
            const char* failed_step = nullptr;

            if (!prepare_pair(peer0, peer1, MultiplayerRespawnMode::RespawnAtEntrance) ||
                !KillSmokePlayer(peer0.state, 1, failed_step) ||
                !KillSmokePlayer(peer1.state, 1, failed_step)) {
                std::cerr << "input lockstep respawn-policy smoke failed during easy setup: "
                          << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                return false;
            }
            StepSingleTick(peer0.state, peer0_audio, peer0_graphics);
            StepSingleTick(peer1.state, peer1_audio, peer1_graphics);
            const bool peer0_p1_alive = SmokePlayerIsAlive(peer0.state, 1);
            const bool peer1_p1_alive = SmokePlayerIsAlive(peer1.state, 1);
            if (!peer0_p1_alive || !peer1_p1_alive) {
                std::cerr << "input lockstep respawn-policy smoke failed: easy respawn did not revive player 1"
                          << " peer0_alive=" << peer0_p1_alive
                          << " peer1_alive=" << peer1_p1_alive << '\n';
                return false;
            }
            if (!CompareCanonicalFingerprints(peer0.state, peer1.state, "input lockstep easy respawn")) {
                std::cerr << "input lockstep respawn-policy smoke failed: easy respawn state diverged"
                          << "\n  first simple diff: "
                          << DescribeFirstStateDifference(peer0.state, peer1.state) << '\n';
                return false;
            }

            if (!prepare_pair(peer0, peer1, MultiplayerRespawnMode::GenerousNextLevel) ||
                !KillSmokePlayer(peer0.state, 1, failed_step) ||
                !KillSmokePlayer(peer1.state, 1, failed_step) ||
                !RunSmokeStageTransition(
                    peer0.state,
                    peer1.state,
                    peer0_audio,
                    peer1_audio,
                    peer0_graphics,
                    peer1_graphics,
                    65432U
                )) {
                std::cerr << "input lockstep respawn-policy smoke failed during generous transition: "
                          << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                return false;
            }
            if (!SmokePlayerIsAlive(peer0.state, 1) || !SmokePlayerIsAlive(peer1.state, 1) ||
                !SmokePlayerIsAlive(peer0.state, 2) || !SmokePlayerIsAlive(peer1.state, 2) ||
                !CompareCanonicalFingerprints(peer0.state, peer1.state, "input lockstep generous respawn")) {
                std::cerr << "input lockstep respawn-policy smoke failed: generous transition did not revive all players\n";
                return false;
            }

            if (!prepare_pair(peer0, peer1, MultiplayerRespawnMode::NoRespawn) ||
                !KillSmokePlayer(peer0.state, 1, failed_step) ||
                !KillSmokePlayer(peer1.state, 1, failed_step) ||
                !RunSmokeStageTransition(
                    peer0.state,
                    peer1.state,
                    peer0_audio,
                    peer1_audio,
                    peer0_graphics,
                    peer1_graphics,
                    76543U
                )) {
                std::cerr << "input lockstep respawn-policy smoke failed during no-respawn transition: "
                          << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                return false;
            }
            if (!SmokePlayerHasNoActiveBody(peer0.state, 1) ||
                !SmokePlayerHasNoActiveBody(peer1.state, 1) ||
                !SmokePlayerIsAlive(peer0.state, 2) ||
                !SmokePlayerIsAlive(peer1.state, 2) ||
                !CompareCanonicalFingerprints(peer0.state, peer1.state, "input lockstep no respawn")) {
                std::cerr << "input lockstep respawn-policy smoke failed: no-respawn transition revived or lost the wrong player\n";
                return false;
            }

            if (!prepare_pair(peer0, peer1, MultiplayerRespawnMode::GenerousNextLevel) ||
                !KillSmokePlayer(peer0.state, 1, failed_step) ||
                !KillSmokePlayer(peer0.state, 2, failed_step) ||
                !KillSmokePlayer(peer1.state, 1, failed_step) ||
                !KillSmokePlayer(peer1.state, 2, failed_step)) {
                std::cerr << "input lockstep respawn-policy smoke failed during game-over setup: "
                          << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                return false;
            }
            peer0.state.SetMode(Mode::GameOver);
            peer1.state.SetMode(Mode::GameOver);
            peer0.state.scene_frame = 60;
            peer1.state.scene_frame = 60;
            InputFrame confirm = InputFrame::New();
            confirm.jump = true;
            ApplyLockstepInputsToState(
                peer0.state,
                std::vector<PlayerId>{1, 2},
                std::vector<InputFrame>{InputFrame::New(), confirm}
            );
            ApplyLockstepInputsToState(
                peer1.state,
                std::vector<PlayerId>{1, 2},
                std::vector<InputFrame>{InputFrame::New(), confirm}
            );
            StepSingleTick(peer0.state, peer0_audio, peer0_graphics);
            StepSingleTick(peer1.state, peer1_audio, peer1_graphics);
            if (peer0.state.mode != Mode::Playing ||
                peer1.state.mode != Mode::Playing ||
                !SmokePlayerIsAlive(peer0.state, 1) ||
                !SmokePlayerIsAlive(peer0.state, 2) ||
                !SmokePlayerIsAlive(peer1.state, 1) ||
                !SmokePlayerIsAlive(peer1.state, 2) ||
                !SmokePlayerOwnershipMatches(peer0.state, 1, PlayerConnectionKind::Local, true) ||
                !SmokePlayerOwnershipMatches(peer0.state, 2, PlayerConnectionKind::Remote, false) ||
                !SmokePlayerOwnershipMatches(peer1.state, 1, PlayerConnectionKind::Remote, false) ||
                !SmokePlayerOwnershipMatches(peer1.state, 2, PlayerConnectionKind::Local, true) ||
                !CompareCanonicalFingerprints(
                    peer0.state,
                    peer1.state,
                    "input lockstep game-over restart"
                )) {
                std::cerr << "input lockstep respawn-policy smoke failed: game-over restart was not deterministic\n";
                std::cerr << "  peer0 mode=" << static_cast<int>(peer0.state.mode)
                          << " peer1 mode=" << static_cast<int>(peer1.state.mode)
                          << " p0_alive=(" << SmokePlayerIsAlive(peer0.state, 1)
                          << "," << SmokePlayerIsAlive(peer0.state, 2)
                          << ") p1_alive=(" << SmokePlayerIsAlive(peer1.state, 1)
                          << "," << SmokePlayerIsAlive(peer1.state, 2)
                          << ")\n"
                          << "  first simple diff: "
                          << DescribeFirstStateDifference(peer0.state, peer1.state) << '\n';
                return false;
            }

            if (!prepare_pair(peer0, peer1, MultiplayerRespawnMode::GenerousNextLevel) ||
                !KillSmokePlayer(peer0.state, 1, failed_step) ||
                !KillSmokePlayer(peer0.state, 2, failed_step) ||
                !KillSmokePlayer(peer1.state, 1, failed_step) ||
                !KillSmokePlayer(peer1.state, 2, failed_step) ||
                !RemoveSmokePlayerBody(peer0.state, 1, failed_step) ||
                !RemoveSmokePlayerBody(peer0.state, 2, failed_step) ||
                !RemoveSmokePlayerBody(peer1.state, 1, failed_step) ||
                !RemoveSmokePlayerBody(peer1.state, 2, failed_step)) {
                std::cerr << "input lockstep respawn-policy smoke failed during missing-body game-over setup: "
                          << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                return false;
            }
            peer0.state.SetMode(Mode::GameOver);
            peer1.state.SetMode(Mode::GameOver);
            peer0.state.scene_frame = 60;
            peer1.state.scene_frame = 60;
            ApplyLockstepInputsToState(
                peer0.state,
                std::vector<PlayerId>{1, 2},
                std::vector<InputFrame>{InputFrame::New(), confirm}
            );
            ApplyLockstepInputsToState(
                peer1.state,
                std::vector<PlayerId>{1, 2},
                std::vector<InputFrame>{InputFrame::New(), confirm}
            );
            StepSingleTick(peer0.state, peer0_audio, peer0_graphics);
            StepSingleTick(peer1.state, peer1_audio, peer1_graphics);
            if (!SmokePlayerOwnershipMatches(peer0.state, 1, PlayerConnectionKind::Local, true) ||
                !SmokePlayerOwnershipMatches(peer0.state, 2, PlayerConnectionKind::Remote, false) ||
                !SmokePlayerOwnershipMatches(peer1.state, 1, PlayerConnectionKind::Remote, false) ||
                !SmokePlayerOwnershipMatches(peer1.state, 2, PlayerConnectionKind::Local, true)) {
                std::cerr << "input lockstep respawn-policy smoke failed: missing-body restart changed slot ownership\n"
                          << "  peer0 ownership:" << DescribeSmokePlayerOwnership(peer0.state) << '\n'
                          << "  peer1 ownership:" << DescribeSmokePlayerOwnership(peer1.state) << '\n';
                return false;
            }

            if (!prepare_pair(peer0, peer1, MultiplayerRespawnMode::GenerousNextLevel) ||
                !RunNetworkFreshQuestReloadSmoke(
                    peer0.state,
                    peer1.state,
                    peer0_audio,
                    peer1_audio,
                    peer0_graphics,
                    peer1_graphics,
                    87654U
                ) ||
                !SmokePlayerIsAlive(peer0.state, 1) ||
                !SmokePlayerIsAlive(peer0.state, 2) ||
                !SmokePlayerIsAlive(peer1.state, 1) ||
                !SmokePlayerIsAlive(peer1.state, 2) ||
                !SmokePlayerOwnershipMatches(peer0.state, 1, PlayerConnectionKind::Local, true) ||
                !SmokePlayerOwnershipMatches(peer0.state, 2, PlayerConnectionKind::Remote, false) ||
                !SmokePlayerOwnershipMatches(peer1.state, 1, PlayerConnectionKind::Remote, false) ||
                !SmokePlayerOwnershipMatches(peer1.state, 2, PlayerConnectionKind::Local, true)) {
                std::cerr << "input lockstep respawn-policy smoke failed: network fresh reload changed ownership\n"
                          << "  peer0 ownership:" << DescribeSmokePlayerOwnership(peer0.state) << '\n'
                          << "  peer1 ownership:" << DescribeSmokePlayerOwnership(peer1.state) << '\n';
                return false;
            }

            std::cout << "input lockstep respawn-policy smoke ok\n";
            return true;
        };

        if (!run_respawn_policy_case()) {
            return false;
        }

        if (!RunRollbackRepairSmoke()) {
            return false;
        }
        if (!RunLockstepHashExchangeSmoke()) {
            return false;
        }
        if (!RunLockstepStageTransitionResyncBlockSmoke()) {
            return false;
        }
        if (!RunLockstepHashRollbackRepairSmoke()) {
            return false;
        }
        if (!RunRollbackLatencySmoke()) {
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "input lockstep smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks
