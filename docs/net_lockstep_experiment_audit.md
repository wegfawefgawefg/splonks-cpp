# net-lockstep-experiment Branch Audit

## Summary

`origin/net-lockstep-experiment` was audited after `integrating-distribution`
was fast-forwarded into `origin/master`.

Do not merge `net-lockstep-experiment` directly. Its tip is older than current
master's Gubsy/lobby integration and would delete current files such as
`src/gubsy_shell.cpp`, `docs/refining-lobbies.md`, default Gubsy data, and the
network restart fixes.

## Branch State

Compared with `origin/master`:

- `integrating-distribution`: no unique commits.
- `origin/integrating-distribution`: no unique commits.
- `origin/lockstep`: no unique commits.
- `origin/net-lockstep-experiment`: 152 commits ahead and 44 commits behind.

The 152 commits are the old distribution/onboarding series. The 44 commits on
master are the later Gubsy menu/lobby integration, lobby refinement docs,
network restart fixes, and default Gubsy data.

## Distribution Work Recovery

The old branch touched 85 files after the divergence point. Every one of those
files was also touched on master, primarily by:

- `be5a488 Integrate distribution work onto Gubsy menu branch`

After comparing `origin/net-lockstep-experiment` against current `origin/master`:

- 81 of the 85 old-branch-touched files are byte-identical or effectively
  recovered in master.
- The only old-branch-touched files that still differed were:
  - `CMakeLists.txt`
  - `scripts/run.sh`
  - `src/cli.cpp`
  - `src/main.cpp`

`CMakeLists.txt` and `src/cli.cpp` differ because master contains newer
Gubsy/lobby integration and smoke checks. The old branch does not contain those
changes.

## Recovered Missing Runtime Details

The audit found two real distribution/runtime details from
`net-lockstep-experiment` that were lost during the later Gubsy startup refactor:

- `scripts/run.sh` no longer forwarded command-line arguments or
  `--project-root`.
- `src/main.cpp` no longer honored `--project-root`, `--content-root`, or
  `SPLONKS_PROJECT_ROOT`, even though Android still launches native code with
  `--project-root`.
- `src/main.cpp` no longer exposed the Android `SDL_main` wrapper from the old
  branch.

These were ported back onto master while preserving the current Gubsy-owned
startup path.

## Remaining Differences Are Expected

Current master intentionally keeps work not present on
`net-lockstep-experiment`:

- Gubsy-owned title/menu shell.
- Gubsy bind/profile defaults.
- Direct host/join menu flow.
- Lobby refinement plan.
- Default dev debug control.
- Host-triggered network restart scheduling and ownership preservation.
- Host-side input ownership guard.
- Focused restart ownership smoke coverage.

The endpoint diff from `net-lockstep-experiment` to master also shows
`docs/integrating_distribution_plan.md` and `docs/refining-lobbies.md` as master
additions. Those are current planning artifacts and should be kept.

## Recommendation

Treat `net-lockstep-experiment` as audited historical backup, not an active
integration branch.

Before deleting it remotely, keep this note and confirm the recovery commit that
restored `--project-root`/`SDL_main` has passed local validation and is pushed.
