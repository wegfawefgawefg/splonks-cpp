# Runtime Content IDs

Built-in content currently uses closed C++ enums like `EntityType` and `Tile`.
Those enums are fast and simple for engine/content code, but they cannot be
extended by runtime-loaded mods.

Data files already use stable string names:

```yaml
entity: skeleton
tile: cave_dirt
```

Those names now resolve through content-name lookup instead of quest-local
switches. Today the resolved runtime ID is still a C++ enum value. For mods,
the resolved runtime ID should become an integer registry ID.

Likely future shape:

```cpp
using EntityArchetypeId = std::uint32_t;
using TileArchetypeId = std::uint32_t;
```

Built-in content can keep named constants:

```cpp
namespace entity_ids {
constexpr EntityArchetypeId Player = 1;
constexpr EntityArchetypeId Skeleton = 2;
}
```

Runtime mods can then register additional archetypes after built-ins:

```text
mod:cool_enemy -> 4001
```

The important boundary is that serialized/data identity should be string-based,
while runtime identity should be a compact ID. Existing C++ enums can be treated
as built-in IDs until the engine is ready to replace them with registry IDs.

Migration implications:

- Entities should eventually store `EntityArchetypeId`, not a closed enum.
- Tiles should eventually store `TileArchetypeId`, not a closed enum.
- Archetype tables should become growable registries instead of fixed arrays.
- Built-in C++ code can still use constants for readability.
- Data files should not depend on enum integer values.
- The content-name resolver is the compatibility layer between stable data names
  and current runtime IDs.
