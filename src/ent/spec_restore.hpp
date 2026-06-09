#pragma once

#include "ent.hpp"

namespace splonks {

void RestoreEntDetachedCarryStateFromSpec(Ent& ent);
void RestoreEntStageEntryStateFromSpec(Ent& ent);
void RestoreEntStoneStateFromSpec(Ent& ent);
void RestoreEntRuntimeCallbacksFromSpec(Ent& ent);

} // namespace splonks
