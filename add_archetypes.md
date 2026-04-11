# Add Archetypes

## Phases

1. Add archetype infrastructure.
Create the shared `EntityArchetype` type, the apply function, and the central registry interface without changing entity behavior yet.

2. Wire `SetEntityByType` to the archetype path.
Make the default spawn entrypoint go through `SetEntityAs` / `ApplyEntityArchetype`, even if the registry is still incomplete at first.

3. Convert each `SetEntityX` into archetype data, one entity at a time.
Lift the default fields out of each constructor function into a `const` archetype definition near that entity’s code. Keep behavior code untouched.

4. Register each archetype in the central table.
As each entity gets an archetype, add its `EntityType -> archetype` mapping in the registry.

5. Replace thin `SetEntityX` wrappers with direct archetype use.
Once a type is fully represented in the table, remove or collapse boilerplate setters that only did default initialization.

6. Keep grouped helpers as thin adapters.
Helpers like money/container-style grouped constructors can remain, but they should just delegate to the table or choose a type and then delegate.

7. Sweep callsites outside `entities/`.
Find direct `SetEntityX(...)` usages in stage init, spawn code, tool use, breakaway spawns, and other systems, and replace the ones that should just use the table.

8. Clean up duplicates and dead mapping code.
Remove setup-fn tables, redundant default animation helpers, and any constructor boilerplate that the archetype registry made obsolete.

9. Final consistency pass.
Check that per-type defaults, animation selection, and special-case entities still have one obvious ownership point, then fix compile errors and regressions at the end.
