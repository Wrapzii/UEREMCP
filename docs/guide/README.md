# Guides

**Owner:** WS-13. Tip baseline: full-use integration (post-hardening + visual
capture + tool-selection). Statuses mirror
[`docs/CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) — **ready for practical
use within cataloged partial scopes; not production-perfect.** Do not equate
pixel-delta PASS with correct appearance or a full metrics close.

**Fresh agents — start here for tool choice:**
[`tool-selection-policy.md`](tool-selection-policy.md) and the machine contract
[`tool-selection-contract.json`](tool-selection-contract.json). Prefer UEREMCP
goal-level tools for create/modify/validate; use Epic for read-only gaps. This does
not claim agents can be forced — prioritization comes from accurate contracts.

These guides are the agent usage / limitations handoff for currently implemented
capabilities. Prefer this tree over guessing from ADRs alone when you are about to
call a tool.

## Guides

| Guide | Audience | Covers |
|---|---|---|
| [`tool-selection-policy.md`](tool-selection-policy.md) | AI agents | **Routing policy** — when to pick which UEREMCP tool vs Epic |
| [`tool-selection-contract.json`](tool-selection-contract.json) | machines / tests | Inventory, prefer_for tags, intent→tool benchmark |
| [`agent-usage.md`](agent-usage.md) | AI agents | Discovery (full MCP toolset names), request envelope, one semantic operation, `dry_run`, `idempotency_key`, `expected_revision`, timeout / poll, visual capture |
| [`capability-reference.md`](capability-reference.md) | agents + humans | Worked examples for live tools on this tip; links to canonical fixtures + [`examples/`](examples/) |
| [`limitations.md`](limitations.md) | both | Honest ceilings — Blueprint scope, B10 vs perfection, visual capture, metrics, security adoption, cooperative cancel and immutable Epic adapter limit, durable idempotency caveats |
| [`troubleshooting.md`](troubleshooting.md) | both | Status vocabulary, manifests, `validation` / diagnostics, next-tool guidance |
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
