# Ent-Local Hitstop Notes

This is a design note for a possible future local hitstop system. It is not implemented.

## Goal

Add short per-ent hitstop without freezing the whole game.

## Recommended Direction

Use a per-ent counter such as `hitstop_frames`, but do not skip the entire ent step.

During local hitstop, the better model is:
- Skip kinematic integration for that ent.
- Do not advance anim.
- Optionally pause logic timers that should visually freeze.
- Keep the ent queryable for collision and contact.
- Still run the minimum maintenance needed so grounding, contact, and attach relationships do not desync badly.

In practice, the important part is to freeze:
- `pos += vel`
- `vel += acc`

and avoid treating the ent as removed from simulation.

## Why Not Skip All Physics

Skipping the full physics step is riskier because:
- moving platforms can desync from riders
- held or attached ents can drift logically
- contact state can go stale
- other ents may continue moving around a frozen body in inconsistent ways

## Working Mental Model

Local hitstop should behave like a frozen kinematic shell:
- the body still exists
- other systems can still see it
- its own advancement is paused

## Edge Cases To Revisit Later

If this is implemented, attach chains probably need special handling so a frozen rider, holder, or carried object does not immediately look broken.
