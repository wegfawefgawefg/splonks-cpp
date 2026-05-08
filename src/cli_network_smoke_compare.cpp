#include "cli_network_smoke_internal.hpp"

#include "entity.hpp"
#include "state.hpp"
#include "state_fingerprint.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace splonks {

namespace {

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

    if (left.entity_manager.entities.size() != right.entity_manager.entities.size()) {
        std::ostringstream output;
        output << "entity array size differs: left=" << left.entity_manager.entities.size()
               << " right=" << right.entity_manager.entities.size();
        return output.str();
    }

    for (std::size_t i = 0; i < left.entity_manager.entities.size(); ++i) {
        const Entity& a = left.entity_manager.entities[i];
        const Entity& b = right.entity_manager.entities[i];
        if (a.active != b.active || a.type_ != b.type_) {
            std::ostringstream output;
            output << "entity " << i << " identity differs:"
                   << " active " << a.active << "/" << b.active
                   << " type " << static_cast<int>(a.type_) << "/"
                   << static_cast<int>(b.type_);
            return output.str();
        }
        if (!a.active) {
            continue;
        }
        if (a.pos != b.pos ||
            a.vel != b.vel ||
            a.acc != b.acc ||
            a.size != b.size ||
            a.rotation != b.rotation ||
            a.health != b.health ||
            a.money != b.money ||
            a.back_vid != b.back_vid ||
            a.holding_vid != b.holding_vid ||
            a.held_by_vid != b.held_by_vid ||
            a.entity_a != b.entity_a ||
            a.entity_b != b.entity_b ||
            a.entity_c != b.entity_c ||
            a.entity_d != b.entity_d ||
            a.grounded != b.grounded ||
            a.holding != b.holding ||
            a.wanted != b.wanted ||
            a.render_enabled != b.render_enabled ||
            a.has_physics != b.has_physics ||
            a.can_collide != b.can_collide ||
            a.attachment_mode != b.attachment_mode ||
            a.movement_flags != b.movement_flags ||
            a.marked_for_destruction != b.marked_for_destruction ||
            a.facing != b.facing ||
            a.draw_layer != b.draw_layer ||
            a.condition != b.condition ||
            a.ai_state != b.ai_state ||
            a.counter_a != b.counter_a ||
            a.counter_b != b.counter_b ||
            a.counter_c != b.counter_c ||
            a.counter_d != b.counter_d ||
            a.point_a != b.point_a ||
            a.point_b != b.point_b ||
            a.point_c != b.point_c ||
            a.point_d != b.point_d ||
            a.stage_exit_id != b.stage_exit_id ||
            a.stage_spawn_index != b.stage_spawn_index ||
            a.fall_timer != b.fall_timer ||
            a.coyote_time != b.coyote_time ||
            a.stun_timer != b.stun_timer ||
            a.frame_data_animator.animation_id != b.frame_data_animator.animation_id ||
            a.frame_data_animator.current_frame != b.frame_data_animator.current_frame ||
            a.frame_data_animator.current_time != b.frame_data_animator.current_time ||
            a.frame_data_animator.speed != b.frame_data_animator.speed ||
            a.frame_data_animator.animate != b.frame_data_animator.animate ||
            a.frame_data_animator.loop != b.frame_data_animator.loop ||
            a.frame_data_animator.finished != b.frame_data_animator.finished) {
            std::ostringstream output;
            output << "entity " << i << " differs:"
                   << " active " << a.active << "/" << b.active
                   << " type " << static_cast<int>(a.type_) << "/"
                   << static_cast<int>(b.type_)
                   << " vidver " << a.vid.version << "/" << b.vid.version
                   << " pos " << a.pos.x << "," << a.pos.y
                   << "/" << b.pos.x << "," << b.pos.y
                   << " vel " << a.vel.x << "," << a.vel.y
                   << "/" << b.vel.x << "," << b.vel.y
                   << " acc " << a.acc.x << "," << a.acc.y
                   << "/" << b.acc.x << "," << b.acc.y
                   << " size " << a.size.x << "," << a.size.y
                   << "/" << b.size.x << "," << b.size.y
                   << " rotation " << a.rotation << "/" << b.rotation
                   << " health " << a.health << "/" << b.health
                   << " money " << a.money << "/" << b.money
                   << " links back/holding/held "
                   << (a.back_vid.has_value() ? static_cast<int>(a.back_vid->id) : -1)
                   << "," << (a.holding_vid.has_value() ? static_cast<int>(a.holding_vid->id) : -1)
                   << "," << (a.held_by_vid.has_value() ? static_cast<int>(a.held_by_vid->id) : -1)
                   << "/"
                   << (b.back_vid.has_value() ? static_cast<int>(b.back_vid->id) : -1)
                   << "," << (b.holding_vid.has_value() ? static_cast<int>(b.holding_vid->id) : -1)
                   << "," << (b.held_by_vid.has_value() ? static_cast<int>(b.held_by_vid->id) : -1)
                   << " grounded " << a.grounded << "/" << b.grounded
                   << " holding " << a.holding << "/" << b.holding
                   << " wanted " << a.wanted << "/" << b.wanted
                   << " render " << a.render_enabled << "/" << b.render_enabled
                   << " physics " << a.has_physics << "/" << b.has_physics
                   << " collide " << a.can_collide << "/" << b.can_collide
                   << " marked " << a.marked_for_destruction
                   << "/" << b.marked_for_destruction
                   << " attachment " << static_cast<int>(a.attachment_mode)
                   << "/" << static_cast<int>(b.attachment_mode)
                   << " facing " << static_cast<int>(a.facing)
                   << "/" << static_cast<int>(b.facing)
                   << " layer " << static_cast<int>(a.draw_layer)
                   << "/" << static_cast<int>(b.draw_layer)
                   << " condition " << static_cast<int>(a.condition)
                   << "/" << static_cast<int>(b.condition)
                   << " ai " << static_cast<int>(a.ai_state)
                   << "/" << static_cast<int>(b.ai_state)
                   << " movement " << a.movement_flags << "/" << b.movement_flags
                   << " counters " << a.counter_a << "," << a.counter_b
                   << "," << a.counter_c << "," << a.counter_d
                   << "/" << b.counter_a << "," << b.counter_b
                   << "," << b.counter_c << "," << b.counter_d
                   << " points " << a.point_a.x << "," << a.point_a.y
                   << ";" << a.point_b.x << "," << a.point_b.y
                   << ";" << a.point_c.x << "," << a.point_c.y
                   << ";" << a.point_d.x << "," << a.point_d.y
                   << "/" << b.point_a.x << "," << b.point_a.y
                   << ";" << b.point_b.x << "," << b.point_b.y
                   << ";" << b.point_c.x << "," << b.point_c.y
                   << ";" << b.point_d.x << "," << b.point_d.y
                   << " stage_exit " << a.stage_exit_id
                   << "/" << b.stage_exit_id
                   << " fall/coyote/stun " << a.fall_timer << ","
                   << a.coyote_time << "," << a.stun_timer << "/"
                   << b.fall_timer << "," << b.coyote_time << "," << b.stun_timer
                   << " anim " << a.frame_data_animator.animation_id
                   << "/" << b.frame_data_animator.animation_id
                   << " frame/time/speed " << a.frame_data_animator.current_frame
                   << "," << a.frame_data_animator.current_time
                   << "," << a.frame_data_animator.speed
                   << "/" << b.frame_data_animator.current_frame
                   << "," << b.frame_data_animator.current_time
                   << "," << b.frame_data_animator.speed
                   << " anim flags " << a.frame_data_animator.animate
                   << "," << a.frame_data_animator.loop
                   << "," << a.frame_data_animator.finished
                   << "/" << b.frame_data_animator.animate
                   << "," << b.frame_data_animator.loop
                   << "," << b.frame_data_animator.finished;
            return output.str();
        }
    }

    if (left.players.slots.size() != right.players.slots.size()) {
        std::ostringstream output;
        output << "player slot count differs: left=" << left.players.slots.size()
               << " right=" << right.players.slots.size();
        return output.str();
    }

    return "no simple lane difference found; fingerprint includes a field not covered by the smoke diff";
}

} // namespace

bool CompareProtocolSmokeStates(const State& coordinator, const State& peer, const char* label) {
    if (CompareCanonicalFingerprints(coordinator, peer, label)) {
        return true;
    }
    std::cerr << "  first simple diff: "
              << DescribeFirstStateDifference(coordinator, peer) << '\n';
    return false;
}

} // namespace splonks
