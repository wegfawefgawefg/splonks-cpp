# Legacy Authoritative Networking Docs

These documents describe the previous coordinator-authoritative /
Terraria-style mutation-replication attempt.

They are preserved for reference only. They are not the active multiplayer plan.
The active plan is:

- `docs/plans/input_lockstep_rollback_plan.md`
- `docs/plans/remote_multiplayer_plan.md`

Do not add new work to these legacy checklists unless documenting why the old
approach was abandoned.

Reason for archiving:

- The old model required too many gameplay/content systems to understand
  networking authority, request/apply paths, prediction, repair, and
  presentation sync.
- That made new content and future mods likely to need custom networking work.
- The new experiment is deterministic input lockstep first, with rollback added
  after deterministic replay is proven.

