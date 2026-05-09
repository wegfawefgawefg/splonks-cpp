#pragma once

namespace splonks {

struct State;

namespace network {

void EnqueueWorldSnapshotEvents(State& state);
void ForceWorldSnapshotResync(State& state);

} // namespace network
} // namespace splonks
