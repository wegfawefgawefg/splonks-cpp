#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace splonks {
struct State;
}

namespace splonks::debug {

class DebugControlServer {
public:
    DebugControlServer() = default;
    DebugControlServer(const DebugControlServer&) = delete;
    DebugControlServer& operator=(const DebugControlServer&) = delete;
    DebugControlServer(DebugControlServer&&) = delete;
    DebugControlServer& operator=(DebugControlServer&&) = delete;
    ~DebugControlServer();

    bool Start(std::uint16_t port, std::string* error_out);
    void Stop();
    void Step(State& state);

    bool IsRunning() const;
    std::uint16_t Port() const;

private:
    struct Client {
        int fd = -1;
        std::string read_buffer;
        std::string write_buffer;
        bool close_after_write = false;
    };

    int listen_fd_ = -1;
    std::uint16_t port_ = 0;
    std::vector<Client> clients_;
};

} // namespace splonks::debug
