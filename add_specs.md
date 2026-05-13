# Add Specs

## Phases

1. Add spec infrastructure.
Create the shared `EntSpec` type, the apply function, and the central registry interface without changing ent behavior yet.

2. Wire `SetEntByType` to the spec path.
Make the default spawn entrypoint go through `SetEntAs` / `ApplyEntSpec`, even if the registry is still incomplete at first.

3. Convert each `SetEntX` into spec data, one ent at a time.
Lift the default fields out of each constructor function into a `const` spec definition near that ent’s code. Keep behavior code untouched.

4. Register each spec in the central table.
As each ent gets an spec, add its `EntType -> spec` mapping in the registry.

5. Replace thin `SetEntX` wrappers with direct spec use.
Once a type is fully represented in the table, remove or collapse boilerplate setters that only did default initialization.

6. Keep grouped helpers as thin adapters.
Helpers like money/container-style grouped constructors can remain, but they should just delegate to the table or choose a type and then delegate.

7. Sweep callsites outside `ents/`.
Find direct `SetEntX(...)` usages in stage init, spawn code, tool use, breakaway spawns, and other systems, and replace the ones that should just use the table.

8. Clean up duplicates and dead mapping code.
Remove setup-fn tables, redundant default anim helpers, and any constructor boilerplate that the spec registry made obsolete.

9. Final consistency pass.
Check that per-type defaults, anim selection, and special-case ents still have one obvious ownership point, then fix compile errors and regressions at the end.
