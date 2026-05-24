#include "gubsy_shell_binds.hpp"

#include "input_bind_schema.hpp"

#include <gubsy/input/types.hpp>
#include <string>

namespace splonks::gubsy_shell {
namespace {

bool has_action(const BindsProfile& profile, int action) {
    return !ginput::button_binds_for_action(profile, action).empty();
}

bool profile_is_playable(const BindsProfile& profile) {
    return has_action(profile, kGubsyActionConfirmJump) &&
           has_action(profile, kGubsyActionMoveUp) && has_action(profile, kGubsyActionMoveDown) &&
           has_action(profile, kGubsyActionMoveLeft) &&
           has_action(profile, kGubsyActionMoveRight) && has_action(profile, kGubsyActionUse);
}

void bind(BindsProfile& profile, GubsyButton button, int action) {
    (void)ginput::add_button_bind(profile, ginput::ButtonBind{static_cast<int>(button), action});
}

BindsProfile build_default_binds(int id, const std::string& name) {
    BindsProfile profile;
    profile.id = id;
    profile.name = name.empty() ? "DefaultBinds" : name;

    bind(profile, GubsyButton::KB_W, kGubsyActionMenuUp);
    bind(profile, GubsyButton::KB_S, kGubsyActionMenuDown);
    bind(profile, GubsyButton::KB_A, kGubsyActionMenuLeft);
    bind(profile, GubsyButton::KB_D, kGubsyActionMenuRight);
    bind(profile, GubsyButton::KB_UP, kGubsyActionMenuUp);
    bind(profile, GubsyButton::KB_DOWN, kGubsyActionMenuDown);
    bind(profile, GubsyButton::KB_LEFT, kGubsyActionMenuLeft);
    bind(profile, GubsyButton::KB_RIGHT, kGubsyActionMenuRight);

    bind(profile, GubsyButton::KB_SPACE, kGubsyActionConfirmJump);
    bind(profile, GubsyButton::KB_ENTER, kGubsyActionConfirmJump);
    bind(profile, GubsyButton::KB_ESCAPE, kGubsyActionBackAttack);
    bind(profile, GubsyButton::KB_H, kGubsyActionBackAttack);
    bind(profile, GubsyButton::KB_M, kGubsyActionPagePreviousBomb);
    bind(profile, GubsyButton::KB_O, kGubsyActionPageNextRope);
    bind(profile, GubsyButton::KB_J, kGubsyActionUse);
    bind(profile, GubsyButton::KB_U, kGubsyActionUse);

    bind(profile, GubsyButton::KB_W, kGubsyActionMoveUp);
    bind(profile, GubsyButton::KB_S, kGubsyActionMoveDown);
    bind(profile, GubsyButton::KB_A, kGubsyActionMoveLeft);
    bind(profile, GubsyButton::KB_D, kGubsyActionMoveRight);

    bind(profile, GubsyButton::GP_DPAD_UP, kGubsyActionMenuUp);
    bind(profile, GubsyButton::GP_DPAD_DOWN, kGubsyActionMenuDown);
    bind(profile, GubsyButton::GP_DPAD_LEFT, kGubsyActionMenuLeft);
    bind(profile, GubsyButton::GP_DPAD_RIGHT, kGubsyActionMenuRight);
    bind(profile, GubsyButton::GP_A, kGubsyActionConfirmJump);
    bind(profile, GubsyButton::GP_B, kGubsyActionBackAttack);
    bind(profile, GubsyButton::GP_X, kGubsyActionPagePreviousBomb);
    bind(profile, GubsyButton::GP_Y, kGubsyActionPageNextRope);
    bind(profile, GubsyButton::GP_DPAD_UP, kGubsyActionMoveUp);
    bind(profile, GubsyButton::GP_DPAD_DOWN, kGubsyActionMoveDown);
    bind(profile, GubsyButton::GP_DPAD_LEFT, kGubsyActionMoveLeft);
    bind(profile, GubsyButton::GP_DPAD_RIGHT, kGubsyActionMoveRight);
    bind(profile, GubsyButton::GP_RIGHT_SHOULDER, kGubsyActionUse);

    return profile;
}

const BindsProfile* find_named_default(GubsyRuntime& runtime) {
    for (const BindsProfile& profile : gubsy_get_binds_profiles(runtime)) {
        if (profile.name == "Default" || profile.name == "DefaultBinds")
            return &profile;
    }
    return nullptr;
}

} // namespace

void EnsureDefaultBinds(GubsyRuntime& runtime) {
    const BindsProfile* existing = find_named_default(runtime);
    if (existing != nullptr && profile_is_playable(*existing))
        return;

    const int profile_id = existing != nullptr ? existing->id : 1;
    const std::string profile_name = existing != nullptr ? existing->name : "DefaultBinds";
    (void)gubsy_replace_binds_profile(runtime, build_default_binds(profile_id, profile_name));
}

} // namespace splonks::gubsy_shell
