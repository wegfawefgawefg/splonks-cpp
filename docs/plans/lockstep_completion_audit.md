# Lockstep Completion Audit

Objective: execute the Net Feel / Rollback / Prediction Plan in
`docs/plans/input_lockstep_rollback_plan.md`, including the
Factorio-aligned target and Track C host-arbitrated skipped input frames.

## Success Criteria

- All implementation checklist items in the active plan section are checked.
- Host-arbitrated skipped input frames are implemented and covered by smoke
  evidence.
- Factorio-style live hash exchange, rollback repair, snapshot resync, and
  join-barrier catchup are implemented and covered by automated evidence.
- Live two-process validation passes required latency profiles without hash
  mismatch, fatal desync, join-barrier stall, or stale confirmed-hash lag.
- Human playtest covers movement, hang, jump, carry/throw, tools, explosives,
  and stage transition under the required fuzzer profiles.
- Human playtest covers high-latency feel and confirms the selected default
  delay/prediction policy is acceptable.

## Artifact Checklist

| Requirement | Evidence | Current Status |
| --- | --- | --- |
| Active plan file exists | `docs/plans/input_lockstep_rollback_plan.md` | Present |
| Human checklist exists | `docs/plans/lockstep_human_playtest_checklist.md` | Present |
| Verdict template exists | `docs/plans/lockstep_human_playtest_verdict_template.json` | Present |
| Telemetry recorder exists | `scripts/record_lockstep_playtest.py` | Present |
| Telemetry summarizer exists | `scripts/summarize_lockstep_playtest.py` | Present |
| Guided playtest runner exists | `scripts/run_lockstep_human_playtest.sh` | Present |
| Verdict helper exists | `scripts/fill_lockstep_playtest_verdict.py` | Present |
| One-command completion auditor exists | `scripts/audit_lockstep_completion.py` | Present |
| Required live automation profiles pass | `logs/lockstep_validate_report.json` from `scripts/validate_lockstep_live.py --launch-pair --profile all --report-json logs/lockstep_validate_report.json` | Present: `ok=true`, profiles `same-house`, `same-city`, `same-state`, `tx-ca`, `ca-fl`, `us-cross-country`, `tx-japan` |
| Human `same-house` telemetry | `logs/lockstep_playtest_*_same-house_summary.json` | Present but too short: latest known duration `0.5s` |
| Human `tx-ca` telemetry | `logs/lockstep_playtest_*_tx-ca_summary.json` | Missing |
| Human `tx-japan` telemetry | `logs/lockstep_playtest_*_tx-japan_summary.json` | Missing |
| Human verdict | `logs/lockstep_playtest_verdict.json` | Missing or unfilled |
| Final verifier | `scripts/summarize_lockstep_playtest.py --verdict-json logs/lockstep_playtest_verdict.json` | Must pass before checking off human gates |

## Current Failing Audit

Current verifier command using the false template:

```bash
scripts/summarize_lockstep_playtest.py --verdict-json docs/plans/lockstep_human_playtest_verdict_template.json
```

Expected current result: fail, because the human evidence is intentionally not
present yet.

Known missing evidence:

- `same-house` needs a full-duration human recording meeting the summary
  minimum.
- `tx-ca` needs a full-duration human recording.
- `tx-japan` needs a full-duration human recording.
- The human verdict JSON must mark every required profile field true only after
  real playtest acceptance.

## Completion Command

Run this to collect the remaining evidence:

```bash
scripts/run_lockstep_human_playtest.sh --launch-pair --fill-verdict
```

The goal can be marked complete only after the final verifier passes with the
filled verdict:

```bash
scripts/summarize_lockstep_playtest.py --verdict-json logs/lockstep_playtest_verdict.json
```

One-command completion audit:

```bash
scripts/audit_lockstep_completion.py
```

Auditor self-test:

```bash
scripts/audit_lockstep_completion.py --self-test
```
