#include "gubsy_shell_smoke.hpp"

#include "gubsy_shell.hpp"

#include <gubsy/runtime.hpp>

#include <iostream>
#include <string>

namespace splonks {

bool CheckGubsyShellSmoke() {
    try {
        State state = State::New();
        gubsy_shell::Shell shell;
        if (!gubsy_shell::InitHeadless(shell, state)) {
            std::cerr << "Gubsy shell smoke failed: InitHeadless failed\n";
            return false;
        }

        std::string message;
        const bool started = gubsy_start_lobby_game(shell.runtime, message);
        gubsy_shell::Shutdown(shell);
        if (!started) {
            std::cerr << "Gubsy shell smoke failed: " << message << '\n';
            return false;
        }
        if (state.mode != Mode::StageTransition) {
            std::cerr << "Gubsy shell smoke failed: lobby start did not enter stage transition\n";
            return false;
        }
        if (state.players.FindPrimaryLocal() == nullptr) {
            std::cerr << "Gubsy shell smoke failed: missing primary local player\n";
            return false;
        }
        if (state.multiplayer_respawn_mode != MultiplayerRespawnMode::GenerousNextLevel) {
            std::cerr << "Gubsy shell smoke failed: lobby config was not applied\n";
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Gubsy shell smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks
