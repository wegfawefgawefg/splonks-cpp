# Docs

- `dev_setup.md`: primary Linux, macOS, Windows, Android, and iOS contributor
  onboarding guide.
- `platform_validation.md`: proof checklist for developer onboarding and real
  release distribution readiness.
- `release_distribution.md`: manual/tag-based release packaging and store
  delivery paths for Linux, macOS, Windows, Android, and iOS.
- `desktop_validation_handoff.md`: copy/paste validation instructions for real
  macOS and Windows machines.
- `../scripts/write_validation_handoff.sh`: writes timestamped Markdown
  validation packets under `dist/validation-handoffs/`.
- `ci_release_policy.md`: policy for keeping GitHub Actions off the normal
  development feedback loop.
- `android_play_release.md`: Google Play internal testing and production
  handoff.
- `asset-metadata-notes.md`: copied from `splonks-rs/docs` as source-port reference.
- `current_architecture_review_checklist.md`: triage checklist for ownership and organization concerns in the current Classic Quest/stagegen work.
- `plans/input_lockstep_rollback_plan.md`: active multiplayer architecture plan: det input lockstep first, rollback after replay determinism is proven.
- `plans/remote_multiplayer_plan.md`: short active remote multiplayer entry point that points to the lockstep/rollback plan.
- `spelunky_physics_research.md`: current player physics vs local Spelunky Classic findings and HD behavior notes.
- `legacy_authoritative_networking/`: archived host-authoritative/Terraria-style networking docs. Reference only; not the active plan.
- `legacy/`: copied planning and notes files from the Rust repo root that still matter during the literal port.
