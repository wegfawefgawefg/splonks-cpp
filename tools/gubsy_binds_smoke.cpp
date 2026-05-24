#include "gubsy_shell_binds.hpp"
#include "input_bind_schema.hpp"

#include <cstdio>
#include <filesystem>
#include <gubsy/runtime.hpp>

namespace {

bool has_action(const BindsProfile& profile, int action) {
    return !ginput::button_binds_for_action(profile, action).empty();
}

bool require_action(const BindsProfile& profile, int action, const char* label) {
    if (has_action(profile, action))
        return true;
    std::fprintf(stderr, "seeded binds profile is missing %s\n", label);
    return false;
}

} // namespace

int main() {
    GubsyRuntime runtime;
    GubsyAppConfig config;
    config.enable_mods = false;
    config.project_root = std::filesystem::current_path().string();
    config.data_root =
        (std::filesystem::current_path() / "build" / "gubsy_binds_smoke_data").string();

    if (!init_gubsy_runtime(runtime, config)) {
        std::fprintf(stderr, "failed to initialize Gubsy runtime\n");
        return 1;
    }

    gubsy_register_binds_schema(runtime, splonks::BuildGubsyBindsSchema());
    splonks::gubsy_shell::EnsureDefaultBinds(runtime);

    const std::vector<BindsProfile>& profiles = gubsy_get_binds_profiles(runtime);
    if (profiles.empty()) {
        std::fprintf(stderr, "no binds profiles after default seeding\n");
        cleanup_gubsy_runtime(runtime);
        return 1;
    }

    const BindsProfile* default_profile = nullptr;
    for (const BindsProfile& profile : profiles) {
        if (profile.name == "Default" || profile.name == "DefaultBinds") {
            default_profile = &profile;
            break;
        }
    }
    if (default_profile == nullptr) {
        std::fprintf(stderr, "default binds profile was not created\n");
        cleanup_gubsy_runtime(runtime);
        return 1;
    }

    bool ok = true;
    ok = require_action(*default_profile, splonks::kGubsyActionConfirmJump, "confirm/jump") && ok;
    ok = require_action(*default_profile, splonks::kGubsyActionMoveUp, "move up") && ok;
    ok = require_action(*default_profile, splonks::kGubsyActionMoveDown, "move down") && ok;
    ok = require_action(*default_profile, splonks::kGubsyActionMoveLeft, "move left") && ok;
    ok = require_action(*default_profile, splonks::kGubsyActionMoveRight, "move right") && ok;
    ok = require_action(*default_profile, splonks::kGubsyActionUse, "use") && ok;

    cleanup_gubsy_runtime(runtime);
    return ok ? 0 : 1;
}
