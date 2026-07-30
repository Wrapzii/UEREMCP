# Guides

**Owner:** WS-13. Tip baseline for this handoff: `ws-11-poc-b10-render`
(`dae0e5c`) plus tool-selection contract (`2026-07-30`). Statuses mirror
[`docs/CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) — **no overall POC-B claim.**

**Fresh agents — start here for tool choice:**
[`tool-selection-policy.md`](tool-selection-policy.md) and the machine contract
[`tool-selection-contract.json`](tool-selection-contract.json). Prefer UEREMCP
goal-level tools for create/modify/validate; use Epic for read-only gaps. This does
not claim agents can be forced — prioritization comes from accurate contracts.

## Guides

| Guide | Audience | Covers |
|---|---|---|
| [`tool-selection-policy.md`](tool-selection-policy.md) | AI agents | **Routing policy** — when to pick which UEREMCP tool vs Epic |
| [`tool-selection-contract.json`](tool-selection-contract.json) | machines / tests | Inventory, prefer_for tags, intent→tool benchmark |
| [`agent-usage.md`](agent-usage.md) | AI agents | Discovery, envelope, one semantic operation, dry_run, revision, jobs |
| [`capability-reference.md`](capability-reference.md) | agents + humans | Worked examples; links to fixtures + [`examples/`](examples/) |
| [`limitations.md`](limitations.md) | both | Honest ceilings — Blueprint scope, B10, metrics, security, cancel |
| [`troubleshooting.md`](troubleshooting.md) | both | Status vocabulary + next-tool guidance |
| [`developer-setup.md`](developer-setup.md) | humans + agents | Local checks that do **not** require a running editor |
| [`template-authoring.md`](template-authoring.md) | humans + agents | Thin pointer into `templates/` + WS-15 authoring rules |

## Contract checks

```bash
python docs/guide/check_guide_links.py
python docs/guide/check_tool_selection_contract.py
python tools/validate_schemas.py
python tools/check_ownership.py --ws WS-13
```

`check_guide_links.py` verifies relative markdown links under `docs/guide/` and that
example fixture paths cited in the guides exist.

`check_tool_selection_contract.py` fails if routing cues, examples, header method
names, or the deterministic intent→tool benchmark regress.

## Source of truth (do not fork)

| Topic | Canonical |
|---|---|
| Tool routing (fresh agents) | [`tool-selection-policy.md`](tool-selection-policy.md) / [`tool-selection-contract.json`](tool-selection-contract.json) |
| Action registry / status | [`docs/CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) |
| Envelope shapes | [`schemas/envelope/`](../../schemas/envelope/) |
| Security / dry_run / tiers | [`docs/SECURITY.md`](../SECURITY.md) |
| POC binary gates | [`docs/POC_ACCEPTANCE.md`](../POC_ACCEPTANCE.md) |
| Cost model | [`docs/WHY.md`](../WHY.md) |
