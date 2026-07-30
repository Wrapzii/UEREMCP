# CLAUDE.md

Read [`AGENTS.md`](AGENTS.md). It is the operating contract for every agent on this
repository, regardless of model or harness, and it is short.

Then, in order:

1. [`docs/WHY.md`](docs/WHY.md) — the problem and the cost model. Counterintuitive;
   read it before making design tradeoffs.
1b. [`docs/guide/tool-selection-policy.md`](docs/guide/tool-selection-policy.md) —
    when calling editor MCP tools, prefer UEREMCP semantic tools for
    create/modify/validate; use Epic for read-only gaps. Machine contract:
    [`docs/guide/tool-selection-contract.json`](docs/guide/tool-selection-contract.json).
    Does not guarantee arbitrary LLM tool choice.
2. [`docs/GROUNDED_FACTS.md`](docs/GROUNDED_FACTS.md) — verified UE 5.8 API surface.
   Cite it rather than re-deriving it.
3. [`docs/adr/`](docs/adr/) — frozen decisions. Do not redesign them in passing.
4. [`docs/WORK_ALLOCATION.md`](docs/WORK_ALLOCATION.md) — your workstream and the paths
   you own.

## The two rules most often broken

**Never claim an Unreal API exists without reading it.** Every API claim carries a
verification tag: `[VERIFIED: path:line]`, `[VERIFIED-RUNTIME: how]`, `[DOCS: url]`, or
`[UNVERIFIED]`. Untagged means unverified. "I recall that `UNiagaraSystem` has…" is not
evidence.

**Edit only paths your workstream owns.** Many agents work this repo at once. To change
something you do not own, write `docs/proposals/<your-ws>-<topic>.md` and keep going.

```bash
python tools/validate_schemas.py
python tools/check_ownership.py --ws WS-nn
```
