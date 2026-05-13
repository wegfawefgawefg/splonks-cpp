# Runtime Content IDs

Built-in content currently uses closed C++ enums like `EntType` and `Tile`.
Those enums are fast and simple for engine/content code, but they cannot be
extended by runtime-loaded mods.

Data files already use stable string names:

```yaml
ent: skeleton
tile: cave_dirt
```

Those names now resolve through content-name lookup instead of quest-local
switches. Today the resolved runtime ID is still a C++ enum value. For mods,
the resolved runtime ID should become an integer registry ID.

Likely future shape:

```cpp
using EntSpecId = std::uint32_t;
using TileSpecId = std::uint32_t;
```

Built-in content can keep named constants:

```cpp
namespace ent_ids {
constexpr EntSpecId Player = 1;
constexpr EntSpecId Skeleton = 2;
}
```

Runtime mods can then register additional specs after built-ins:

```text
mod:cool_enemy -> 4001
```

The important boundary is that serialized/data ident should be string-based,
while runtime ident should be a compact ID. Existing C++ enums can be treated
as built-in IDs until the engine is ready to replace them with registry IDs.

Migration implications:

- Ents should eventually store `EntSpecId`, not a closed enum.
- Tiles should eventually store `TileSpecId`, not a closed enum.
- Spec tables should become growable registries instead of fixed arrays.
- Built-in C++ code can still use constants for readability.
- Data files should not depend on enum integer values.
- The content-name resolver is the compatibility layer between stable data names
  and current runtime IDs.

## Tile Runtime Modding Notes

Current compromise:

- `Tile` is still a closed C++ enum.
- `TileSpec` still describes engine-facing tile behavior for those enum
  values.
- Classic quest stagegen owns classic tile palette mapping such as "the block
  tile matching this cave dirt tile".
- Debug stages use explicit tile enum values.
- Generic engine code should not infer content relationships between tiles.

That is acceptable for built-in content, but it does not support runtime-modded
tiles yet.

Runtime-modded tiles are possible, but they require a real tile registry:

- Replace stored `Tile` values in `Stage` tile grids with a compact
  `TileSpecId`.
- Register built-in tiles first, then append mod tiles.
- Resolve YAML/string names like `cave_dirt` or `my_mod:crystal_block` through
  the registry at load time.
- Move fixed-size tile spec lookup to a growable table.
- Keep engine systems querying spec properties, not tile names or content
  families.
- Keep quest/stagegen-specific concepts like palettes, glyph mappings, and
  "matching block tile" inside quest/content code.

The main risk is that a lot of current code assumes `Tile` is a cheap enum that
can appear in switches. Runtime tiles would need those switches converted to
spec properties, callbacks, or content-side helpers. Until then, modded
tiles can be represented in data only if they map onto existing built-in tile
IDs.
