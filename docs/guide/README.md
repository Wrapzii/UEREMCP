# Guides

**Owner:** WS-13. Tip baseline: post-hardening local main parent `6a611cf`
(documentation certification builds on that tip). Statuses mirror
[`docs/CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) — **POC A–E claimed;
not production-ready.** Do not equate B10 PASS with production visual perfection
or a full metrics close.

These guides are the agent usage / limitations handoff for currently implemented
capabilities. Prefer this tree over guessing from ADRs alone when you are about to
call a tool.

## Guides

| Guide | Audience | Covers |
|---|---|---|
| [`agent-usage.md`](agent-usage.md) | AI agents | Discovery, request envelope, one semantic operation, `dry_run`, `idempotency_key`, `expected_revision`, timeout / poll |
| [`capability-reference.md`](capability-reference.md) | agents + humans | Worked examples for live tools on this tip; links to canonical fixtures (no giant payloads) |
| [`limitations.md`](limitations.md) | both | Honest ceilings — Blueprint scope, B10 vs perfection, metrics, security adoption, cooperative cancel and immutable Epic adapter limit, durable idempotency caveats |
| [`troubleshooting.md`](troubleshooting.md) | both | Status vocabulary, manifests, `validation` / diagnostics |
| [`developer-setup.md`](developer-setup.md) | humans + agents | Local checks that do **not** require a running editor |
| [`template-authoring.md`](template-authoring.md) | humans + agents | Thin pointer into `templates/` + WS-15 authoring rules |

## Contract checks

```bash
python docs/guide/check_guide_links.py
python tools/validate_schemas.py
python tools/check_ownership.py --ws WS-13
```

`check_guide_links.py` verifies relative markdown links under `docs/guide/` and that
example fixture paths cited in the guides exist.

## Source of truth (do not fork)

| Topic | Canonical |
|---|---|
| Action registry / status | [`docs/CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) |
| Envelope shapes | [`schemas/envelope/`](../../schemas/envelope/) |
| Security / dry_run / tiers | [`docs/SECURITY.md`](../SECURITY.md) |
| POC binary gates | [`docs/POC_ACCEPTANCE.md`](../POC_ACCEPTANCE.md) |
| Cost model | [`docs/WHY.md`](../WHY.md) |
