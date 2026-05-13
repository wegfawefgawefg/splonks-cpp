#include "cli_state_smoke.hpp"

#include "ent.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe.hpp"
#include "graphics.hpp"
#include "inputs.hpp"
#include "network/input_lockstep.hpp"
#include "network/net_fuzzer.hpp"
#include "network/net_lobby.hpp"
#include "quest_stage_loader.hpp"
#include "raw_aframe.hpp"
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
#include <cmath>
#include <exception>
#include <iostream>
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
               << " value=" << effect.value
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
                   << " rotation " << a.rotation << "/" << b.rotation
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
                   << " lights " << a.light_strength << "," << a.light_radius
                   << "/" << b.light_strength << "," << b.light_radius
                   << " points " << a.point_a.x << "," << a.point_a.y
                   << "/" << b.point_a.x << "," << b.point_a.y
                   << " anim " << a.aframe_animator.anim_id
                   << "/" << b.aframe_animator.anim_id
                   << " frame " << a.aframe_animator.current_frame
                   << "/" << b.aframe_animator.current_frame
                   << " time " << a.aframe_animator.current_time
                   << "/" << b.aframe_animator.current_time
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
                   << (b.ent_vid.has_value() ? static_cast<int>(b.ent_vid->id) : -1);
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
    Ent* const player = state.ents.GetEntMut(*slot->ent_vid);
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
        std::uint64_t delay_ticks = static_cast<std::uint64_t>(std::ceil(delay_ms / kTickMs));
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
    return true;
}

bool KillSmokePlayer(State& state, PlayerId player_id, const char*& failed_step) {
    const PlayerSlot* const slot = state.players.Find(player_id);
    if (slot == nullptr || !slot->ent_vid.has_value()) {
        failed_step = "find smoke player to kill";
        return false;
    }
    Ent* const player = state.ents.GetEntMut(*slot->ent_vid);
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

void ConfigureSmokeNetworkRoles(State& peer0, State& peer1) {
    peer0.net_session.role = network::NetRole::Host;
    peer1.net_session.role = network::NetRole::Peer;
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
        StepSingleTick(peer.state, audio, graphics);
        const CanonicalStateFingerprint fingerprint =
            ComputeGameplayDeterminismFingerprint(peer.state);
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

} // namespace

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
        return true;
    } catch (const std::exception& e) {
        std::cerr << "det replay smoke failed: " << e.what() << '\n';
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

        const std::vector<std::array<InputFrame, 2>> script =
            BuildInputLockstepSmokeScript();
        const std::vector<PlayerId> required_players = {1, 2};

        const auto run_case = [&](
            const char* label,
            const network::NetFuzzerConfig& fuzzer,
            std::uint32_t fuzzer_seed,
            network::LockstepFrame input_delay_frames
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

                const network::LockstepInputPacket p0_packet =
                    BuildLockstepInputPacket(peer0, script, latest_input_frame);
                const network::LockstepInputPacket p1_packet =
                    BuildLockstepInputPacket(peer1, script, latest_input_frame);
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
                    std::cerr << "input lockstep smoke " << label
                              << " failed while stepping: " << step_error << '\n';
                    return false;
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
            if (!SmokePlayerIsAlive(peer0.state, 1) || !SmokePlayerIsAlive(peer1.state, 1) ||
                !CompareCanonicalFingerprints(peer0.state, peer1.state, "input lockstep easy respawn")) {
                std::cerr << "input lockstep respawn-policy smoke failed: easy respawn did not revive player 1\n";
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

            std::cout << "input lockstep respawn-policy smoke ok\n";
            return true;
        };

        if (!run_respawn_policy_case()) {
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "input lockstep smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks
