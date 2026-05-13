# Contact Dispatch Plan

## Goal

Replace the current ad hoc swept-contact and blocking-impact logic with a small,
explicit contact model that:

- gathers all relevant contacts for an attempted pixel move
- lets contact resolvers decide whether movement is blocked
- lets contact resolvers stop the rest of the sweep when needed
- keeps pair cooldowns for repeated ent contact effects
- does not let cooldown disable base physical blocking

## Core Semantics

Use one small shared result type:

- `blocks_movement`
  - this attempted pixel move may not be entered
  - this does not automatically mean the whole remaining sweep must end
- `stop_sweep`
  - stop all further movement processing for this ent in the current
    `MoveEntPixelStep(...)` call

These are intentionally separate.

Examples:

- solid wall:
  - `blocks_movement = true`
  - `stop_sweep = false`
- bat swept hit:
  - `blocks_movement = false`
  - `stop_sweep = true`
- passable trigger/effect tile:
  - `blocks_movement = false`
  - `stop_sweep = false`

## Contact Gathering

For an attempted pixel move, gather a full blocking contact set:

- stage bounds contact
- all touched tile contacts
- all touched ent contacts

This must not collapse to either/or.

It is valid for a single attempted pixel move to touch:

- stage bounds and tiles
- tiles and ents
- multiple ents

## Resolution Rules

### Swept Ent-to-Ent Contact

This runs after a successful pixel move, like the existing bat contact path.

- gathers the actual touched ent contacts for the current swept pose
- dispatches one ent/ent contact event per touched pair for that pose
- invokes both participants from that same contact event
- aggregates the final result after all touched contacts have run
- uses sided cooldowns for repeated effect application
- may stop the sweep
- does not implicitly block the attempted movement pixel

It must not:

- scan the whole world instead of the touched set
- short-circuit on the first touched contact before later touched contacts run
- only invoke mover-side logic for the pair

### Blocking Contact Resolution

This runs for the attempted next pixel before entering it.

- bounds/tile/ent contacts are all visible to the resolver
- base blocking comes from contact properties, not from cooldown
- cooldown only suppresses repeated effect behavior
- impassable ents remain solid even if repeated effect logic is suppressed

### Blocking Impact Resolution

If blocking contact resolution says the attempted pixel is blocked, a separate
blocking-impact resolver runs with:

- the full blocking contact set
- the impact axis
- the impact direction
- the pre-impact velocity

This is the right place for:

- breakables shattering on impact
- bounce/thud logic
- future tile/ent impact effects

It must dispatch across the whole touched blocking set:

- stage bounds contact
- every touched tile contact
- every touched ent contact

Handler families stay grouped by contact kind:

- one `ent↔ent` dispatcher family
- one `ent↔tile` dispatcher family

It must not collapse to a single primary blocking contact for effect dispatch.

## Current Migration Targets

1. Move bat onto a generic swept ent/ent dispatcher.
2. Move breakaway container impact breakage onto the blocking-impact path.
3. Replace the current single-blocker query with a full contact set.

## Important Constraint

Cooldown must not change solidity.

If an impassable ent is touched:

- it still blocks movement
- even if repeated contact effects are suppressed by cooldown

## Implementation Shape

Keep the layout boring and split by responsibility:

- `common_ent_contact.cpp`
  - ent/ent contact dispatch
- `common_blocking_contact.cpp`
  - attempted blocking contact gathering and resolution
- `common_tile_contact.cpp`
  - ent/tile contact dispatch
- `common_physics.cpp`
  - mover only

This keeps physics readable and keeps the contact model easy to trace in a
debugger.

Type-specific behavior should be selected by type match/dispatch inside the
contact-type handlers, not by piling ent-specific `if` checks directly into
the generic contact loops.

For ent/ent contact, this means:

- gather one contact event for the pair
- invoke participant A's handler for that event
- invoke participant B's handler for that same event
- aggregate the two results
- participant A checks and writes `A -> B` cooldowns only
- participant B checks and writes `B -> A` cooldowns only
- a cooldown written by A must not suppress B in that same contact event

This avoids:

- mover-only behavior
- giant pair-combo resolvers
- live pair-cooldown mutation suppressing the second participant in the same
  event
