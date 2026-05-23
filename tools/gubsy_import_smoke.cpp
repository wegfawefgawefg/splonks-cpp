#include <gubsy/runtime.hpp>

#include <cstdio>

int main() {
    GubsyRuntime runtime{};
    GubsyAppConfig config{};
    config.enable_mods = false;

    if (!init_gubsy_runtime(runtime, config)) {
        std::fprintf(stderr, "failed to initialize Gubsy runtime\n");
        return 1;
    }

    if (gubsy_runtime_config(runtime).enable_mods) {
        std::fprintf(stderr, "Gubsy mods should be disabled for Splonks smoke\n");
        cleanup_gubsy_runtime(runtime);
        return 1;
    }

    if (gubsy_runtime_has_menu_screen(runtime, MenuScreenID::MODS)) {
        std::fprintf(stderr, "Gubsy mod browser screen should not be registered\n");
        cleanup_gubsy_runtime(runtime);
        return 1;
    }

    if (!gubsy_show_main_menu(runtime)) {
        std::fprintf(stderr, "failed to show Gubsy main menu\n");
        cleanup_gubsy_runtime(runtime);
        return 1;
    }

    cleanup_gubsy_runtime(runtime);
    return 0;
}
