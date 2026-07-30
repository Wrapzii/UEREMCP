# RB-13: Security, permissions, and reliability boundaries

- **Owner:** WS-12
- **Status:** complete (Wave 1 research / ADR-0010 input; implementation deferred to Wave 2)
- **Blocks:** ADR-0010
- **Priority:** high
- **Last updated:** 2026-07-29
- **Handoff:** `docs/proposals/ws-12-adr-0010-security-reliability.md`,
  `docs/proposals/ws-12-hazard-list.md`

## Framing

This system will have write access to a real project the owner has been building, and
will be driven by a swarm of agents running unattended. The realistic threat is not an
attacker — it is **an agent confidently destroying work**, at speed, in parallel.

Design accordingly. The controls that matter most are the boring ones: allowed roots,
dry-run defaults, and an audit trail good enough to reconstruct what happened.

**Research constraint this run:** source + non-mutating runtime probes only. No asset
creates/deletes, no sandbox Enter/Persist, no UndoTransaction.

---

## Executive summary

| Control surface | Verdict |
|---|---|
| Epic MCP authentication | **None.** Origin guard only. Fake `Authorization` accepted. |
| Bind address | **Loopback.** Default `localhost`; runtime `127.0.0.1:8000` LISTENING. |
| Origin header | Rejects non-localhost Origin with **403**; no Origin / localhost / 127.0.0.1 / `[::1]` allowed. |
| `FileSandbox` / `FGlobalSandbox` | **Transaction boundary**, not security. Tracks content mount points only; `Saved/` and `Config/` unaffected. |
| Inherited blast radius | Epic `ProgrammaticToolset.execute_tool_script` + asset delete/write tools + console/Python surface = **unsafe-tier** capability already present. |
| Concurrent writers | Transport allows many sessions; **no mutator queue** in Epic or ToolsetRegistry. |
| Modal / freeze | Host-side UnrealWatchMCP only; in-editor tools cannot self-heal when game thread blocked. |
| RE project SCC | **Git** present (`.git` + remote). No Unreal SourceControlSettings.ini found; do not rely on Perforce checkout APIs as the safety net. |
| Envelope `dry_run` | Schema default **`false`** `[VERIFIED: schemas/envelope/request.schema.json:61-64]`; destructive actions must force `true` in ADR-0010 / domain code. |

ADR-0010 recommendation and implementation plan:
`docs/proposals/ws-12-adr-0010-security-reliability.md`.

---

## Questions

### A. What is exposed today

1. Does Epic's MCP server authenticate at all?
2. What interface does it bind to?
3. What can existing Epic toolsets already do (blast radius)?
4. Is `FGlobalSandbox` / `FileSandbox` a security or transaction boundary?

### B. What we must add

5. Allowed project and asset roots.
6. Permission tiers for ADR-0010.
7. Destructive `dry_run` defaults.
8. Audit trail.
9. Source control / safety net.
10. Concurrency hazards / locks.
11. Editor state hazards (PIE, unsaved, modal).
12. `UndoTransaction` hazard.

---

## Findings

### A1. Authentication

| Verdict | **No authentication.** Origin validation only. |

**Source:**

- `ValidateOriginHeader` is the only request gate before JSON-RPC handling
  `[VERIFIED: ModelContextProtocolServer.cpp:69-120, 518-523]`.
- Allows: missing Origin; Origin host `localhost` / `127.0.0.1` / `[::1]`
  `[VERIFIED: ModelContextProtocolServer.cpp:76-115]`.
- Rejects other Origins with HTTP **403**
  `[VERIFIED: ModelContextProtocolServer.cpp:118-120]`.
- Engine tests cover evil / localhost / 127.0.0.1 / no-Origin cases
  `[VERIFIED: ModelContextProtocolEngineSubsystemTests.cpp:954-1025]`.
- No `Authorization` / Bearer / API-key handling under
  `$MCP/Source` (ripgrep empty) `[VERIFIED: grep ModelContextProtocol/Source]`.
- Settings expose only path, port, auto-start, tool-search — no auth fields
  `[VERIFIED: ModelContextProtocolSettings.h:23-46]`.

**Runtime (non-mutating `ping`):**

| Probe | Result |
|---|---|
| No `Origin` | HTTP 200, empty JSON-RPC result `[VERIFIED-RUNTIME: Invoke-WebRequest POST :8000/mcp ping 2026-07-29]` |
| `Origin: http://localhost:3000` | HTTP 200 `[VERIFIED-RUNTIME: same]` |
| `Origin: http://evil.example.com` | HTTP **403**, empty body `[VERIFIED-RUNTIME: same]` |
| `Authorization: Bearer totally-fake` | HTTP **200** — header ignored `[VERIFIED-RUNTIME: same]` |

**Implication:** any local process that can open a TCP connection to loopback can drive
the editor's full tool surface. UEREMCP cannot add transport auth without forking Epic
MCP (ADR-0002 forbids that). Mitigations are **application-layer**: permission tiers,
allowed roots, mutator queue, audit, and operational guidance (loopback-only, do not
rebind to `any`).

### A2. Bind interface

| Verdict | **Loopback by default and in current RE runtime.** |

**Source:**

- MCP starts via `FHttpServerModule::GetHttpRouter(port)` + `StartAllListeners()`
  `[VERIFIED: ModelContextProtocolServer.cpp:432-447]`. No bind-address parameter in
  MCP settings `[VERIFIED: ModelContextProtocolSettings.h:36-42]`.
- HTTP server listener default `BindAddress = "localhost"`
  `[VERIFIED: HttpServerConfig.h:12-13]`.
- `"localhost"` → `SetLoopbackAddress()`; `"any"` → `SetAnyAddress()`
  `[VERIFIED: HttpListener.cpp:64-70]`.
- Client config generation hard-codes `http://127.0.0.1:<port><path>`
  `[VERIFIED: ModelContextProtocolClientConfig.cpp:158]`.
- RE `.mcp.json` points at `http://127.0.0.1:8000/mcp`
  `[VERIFIED: $PROJ/.mcp.json]`.
- RE has `bAutoStartServer=True` (project override of engine default `false`)
  `[VERIFIED: EditorPerProjectUserSettings.ini [/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]]`.

**Runtime:**

```
TCP  127.0.0.1:8000  0.0.0.0:0  LISTENING  <UnrealEditor pid>
TCP  127.0.0.1:8001  0.0.0.0:0  LISTENING  <proxy pid>
```

`[VERIFIED-RUNTIME: netstat -ano 2026-07-29]` — **not** `0.0.0.0:8000`.

**Hazard:** an operator (or ini) can set `DefaultBindAddress=any` under
`[HTTPServer.Listeners]` and expose the unauthenticated MCP surface on the LAN.
ADR-0010 must document: refuse to enable UEREMCP mutators if bind is non-loopback
(detect via `netstat`/public probe API if available; otherwise document as
operator responsibility + startup warning).

**`bAutoStartServer` recommendation:** keep project-level `true` for agent workflows
(matches RE today and WS-04 C14). UEREMCP must not force-start Epic's server; document
that auto-start is required for unattended agents. Default-in-engine remains `false`
`[VERIFIED: ModelContextProtocolSettings.h:42]` — correct for shipping templates;
project override is the right place.

### A3. Inherited blast radius

| Verdict | Agents already inherit powerful mutators via Epic toolsets. UEREMCP must not widen the surface without tiers. |

**`ProgrammaticToolset.execute_tool_script`:**

- Exists and is the batching glue for other tools
  `[VERIFIED: programmatic.py:880-953]`.
- Restricted imports (`json`, `math`, `datetime`, `copy`, `re`, `time` only)
  `[VERIFIED: programmatic.py:52-81]`.
- `open()` replaced with read-only opener constrained to
  `project_dir()` + `project_saved_dir()`
  `[VERIFIED: programmatic.py:84-188]`.
- Still: can call **any registered tool** from the script — including deletes and
  graph mutations — so it is **orchestration RCE over the tool surface**, not OS shell
  RCE. Classify as **`unsafe`** in ADR-0010.
- On error, uses `UToolsetLibrary::UndoTransaction` to roll back script work
  `[VERIFIED: ToolsetLibrary.h:110-129]` — see B12 hazard.

**Asset tools:**

- `AssetTools.delete` deletes assets/directories
  `[VERIFIED: asset.py:159-172]`.
- `read_file` / `write_file` roots: project Content, project Saved, plugin Content
  (including **engine plugins** when `include_engine=True`)
  `[VERIFIED: asset.py:565-571]`.
- Write path may attempt source-control checkout when not writable
  `[VERIFIED: asset.py:545-548]`.

**FileSandbox:**

- Tracks **content mount points only**; `Saved/` and `Config/` not affected
  `[VERIFIED: ISandboxInstance.h:28-30]`. Confirmed by WS-11 runtime gate
  `[VERIFIED-RUNTIME: WS-11 Rollback.MultiAssetDiscard / proposal 2026-07-29]`.

**WS-04 transport:**

- Concurrent sessions OK; no write serialization
  `[VERIFIED: docs/proposals/ws-04-concurrent-clients.md + ModelContextProtocolServer.cpp sessions]`.
- ToolsetRegistry cancel not wired; disconnect may leave work running
  `[VERIFIED: RB-04 §B11, §A7]`.

### A4. Sandbox = transaction, not security

| Verdict | **Transaction / rollback boundary only.** |

Evidence chain:

1. Header states only mount-point I/O is tracked; Saved/Config excluded
   `[VERIFIED: ISandboxInstance.h:28-30]`.
2. ADR-0005 designs Enter → mutate → Persist/Discard as rollback, not ACL
   `[VERIFIED: ADR-0005]`.
3. WS-11: Enter of a *different* name silently `Leave`s the previous sandbox
   (files kept) `[VERIFIED: ws-11-adr-0005-sandbox-semantics.md citing SandboxLibrary.cpp:67-81]`.
4. `DiscardFiles` does not purge/hot-reload packages
   `[VERIFIED: same proposal citing SandboxLibrary.cpp:178-203]`.

**Do not** treat sandbox presence as proof a path was allowed. Path policy lives in
`UeremcpSecurity` and must run **before** sandbox enter.

---

### B5. Allowed roots (recommendation)

Enforce at **three** layers (implementation in Wave 2 `UeremcpSecurity`):

| Layer | Accept | Reject |
|---|---|---|
| Soft object / package path | `/Game/...`, `/<ProjectPlugin>/...` under project plugins | `/Engine/...` writes; `/Temp/...` persistence; empty; `..` segments; absolute Windows paths as package paths |
| Filesystem path | `ProjectDir`, `ProjectContentDir`, `ProjectSavedDir/UEREMCP/**` (plugin-owned), project plugin Content | Outside project tree; other projects' dirs; Engine install tree writes; path traversal after `realpath` |
| Cross-project | `request.project.path` must equal the **currently open** `.uproject` (normalized) | Operations when no project loaded; mismatched path |

Reuse Epic's approach from `programmatic.py` (`commonpath` / realpath) and
`AssetTools._require_allowed_path`, but **tighten**: do not allow engine-plugin Content
writes by default (Epic asset tools do allow them when `include_engine=True`
`[VERIFIED: asset.py:565-571]` — that is too wide for UEREMCP mutators).

Idempotency / audit store writable root (proposal to WS-05):

- Prefer `Saved/UEREMCP/` — **outside** FileSandbox mount tracking, so Discard of a
  failed batch does not erase the idempotency/audit record
  `[VERIFIED: ISandboxInstance.h:28-30]`.
- Do **not** put the store under `Intermediate/Sandboxes/` (sandbox-owned).
- Session-memory store is the Wave 1 minimum (ADR-0006); disk under `Saved/UEREMCP/`
  is the Wave 2 preference for restart survival.

### B6. Permission tiers (ADR-0010 draft)

| Tier | Meaning | Default | Examples |
|---|---|---|---|
| `read` | Inspection only | Always available | graph inspect, describe, list, validate-only |
| `write` | Create/modify inside allowed roots without deleting user content | **Default for UEREMCP tools** | `create_or_update`, `patch`, `repair` on owned paths |
| `destructive` | Delete or replace existing user content | Opt-in per request (`options` or mode) | `delete`, `replace` of existing assets, force overwrite |
| `unsafe` | Arbitrary script / console / OS / engine-wide | **Off** unless explicitly enabled in project settings | `execute_tool_script`, console commands, unrestricted Python |

Enforcement points:

1. **Tool registration / name filters** — hide `unsafe` tools via `SetNameFilters` unless
   settings enable them `[VERIFIED: Toolset.h SetNameFilters per GROUNDED_FACTS §2.2]`.
2. **Envelope gate in UeremcpCore/Protocol** — before domain service: tier vs action/mode.
3. **Path validator** — every target path.
4. **Mutator queue** — serialize `write`+ mutators (see B10).

### B7. Destructive `dry_run` defaults

Envelope schema default is `dry_run: false`
`[VERIFIED: schemas/envelope/request.schema.json:61-64]`, with a note that destructive
actions flip it — reserved for ADR-0010.

**Force `dry_run: true` unless caller sets `dry_run: false` explicitly** for:

| Action / mode | Reason |
|---|---|
| `mode: delete` | Permanent asset loss |
| `mode: replace` when target exists | Wipes user content |
| `mode: rebuild_from_specification` when target exists and would discard non-spec content | Same class as replace |
| Any operation with `deleted_assets` predicted non-empty | Precautionary |

Non-destructive `create` / `create_or_update` / `patch` may keep schema default `false`.

Implementation: central policy table in `UeremcpSecurity` keyed by `(action, mode,
target_exists)` — domains must not reimplement.

### B8. Audit trail

Minimum to answer "what changed in the last hour?":

| Field | Source |
|---|---|
| timestamp (UTC) | wall clock |
| `request_id`, `idempotency_key`, `action`, `mode`, `status` | envelope |
| `session_id` if available | MCP session (may be absent to domain services) |
| `target.asset_path`, created/modified/deleted lists | response `result` / sandbox `GetChanges()` |
| `dry_run`, `atomic`, tier required | options / policy |
| `revision` before/after | ADR-0006 |
| operator machine / project path | `project.path` |

Storage: append-only JSONL under `Saved/UEREMCP/audit/YYYY-MM-DD.jsonl`
(outside sandbox). Retention: configurable, default 14 days. Never claim undo from
audit alone — audit points at change manifests + SCC.

### B9. Source control

| Finding | Evidence |
|---|---|
| RE has a **Git** working tree and remotes | `[VERIFIED-RUNTIME: Test-Path RE/.git; git status/remote 2026-07-29]` |
| No `SourceControlSettings.ini` under RE Saved Config | `[VERIFIED-RUNTIME: path missing]` |
| Epic AssetTools can checkout via Unreal SCC APIs when configured | `[VERIFIED: asset.py:545-548, _sc_state]` |

**Recommendation:** treat Git as the human safety net (commit before swarm runs). Do
**not** block UEREMCP on Unreal SCC being configured. Optional Wave 3: soft-check
`git status --porcelain` for dirty targets and surface `capability_notes` — proposal
to WS-01/WS-13, not a hard gate for POC.

### B10. Concurrency / locking

| Layer | Provides | Gap |
|---|---|---|
| ADR-0006 `expected_revision` / `idempotency_key` | Per-asset optimistic concurrency + retry dedupe | Does not serialize interleaved mutations mid-operation |
| Epic MCP sessions | Many concurrent clients | No global mutator lock `[VERIFIED: ws-04-concurrent-clients.md]` |
| FileSandbox nesting | Single active sandbox; different-name Enter Leaves prior | Concurrent agents can clobber sandbox scope `[VERIFIED: WS-11 proposal]` |

**ADR-0010 recommendation:** UEREMCP-side **single active mutator queue** (project
scope):

- `read` tools: concurrent.
- `write` / `destructive` / `unsafe`: FIFO queue; one active; waiters get
  `partially_completed` + `job` (ADR-0009 poll model from WS-04) or short wait with
  timeout.
- Optional finer lock later: per soft-package path — not required for v1 if global
  mutator queue exists.
- Detect `FGlobalSandbox::IsActive()` before Enter; if active under another name,
  **refuse** (do not Leave) `[aligns ADR-0005 rule 3 + WS-11]`.

### B11. Editor state hazards

| Hazard | Evidence | Mitigation |
|---|---|---|
| Modal / game-thread block | MCP completes on game thread `[VERIFIED: ModelContextProtocolServer.cpp:851-853]`; UnrealWatchMCP is host-side `[VERIFIED: REAgentTools/Optional/UnrealWatchMCP/README.md]` | Document companion `unreal-watch`; never auto-dismiss destructive dialogs; do not put modal detection inside UEREMCP transport (WS-04 proposal) |
| PIE | Not probed this run | Reject mutators during PIE (editor API check in Wave 2) — flag as open until runtime-verified |
| Unsaved user packages | Not probed | Before destructive ops: warn via `capability_notes` / require `dry_run` first |
| MCP hung / CLOSE_WAIT | Observed CLOSE_WAIT on :8000 during session `[VERIFIED-RUNTIME: netstat]` | Align with UnrealWatchMCP habits; do not thrash re-init |

### B12. `UndoTransaction` hazard

| Verdict | **Can undo user work.** Treat as destroy-user-content bug class. |

`UToolsetLibrary::UndoTransaction` calls `GEditor->UndoTransaction` on the **global**
undo stack `[VERIFIED: ToolsetLibrary.cpp:288-294]`. Documented as companion for
`execute_tool_script` rollback `[VERIFIED: ToolsetLibrary.h:110-119]`. Specs show it
undoes whatever is top of stack `[VERIFIED: ToolsetLibraryTest.cpp:1488+]`.

**Rules for UEREMCP:**

1. Never call `UndoTransaction` to "clean up" unless the same call stack sampled
   `GetActiveUndoCount()` before BeginTransaction and only undoes the delta it owns.
2. Prefer `FGlobalSandbox::Discard()` for file-level rollback (ADR-0005) over undo.
3. Do not expose `UndoTransaction` as an agent-facing action.
4. Mark Epic `execute_tool_script` error-path undo as a known inherited hazard when
   `unsafe` is enabled.

WS-11 still lists composition with BP compile as open
`[VERIFIED: RB-06 §C q11-13]` — do not claim transaction+sandbox composition proven
for Blueprint graphs.

---

## Threat model (agent-error centred)

| Threat | Likelihood | Impact | Primary control |
|---|---|---|---|
| Agent deletes / replaces wrong asset | High | High | Destructive dry-run default; allowed roots; `expected_revision` |
| Parallel agents corrupt same asset / sandbox | High | High | Mutator queue; refuse nested sandbox Leave |
| Agent retries create duplicates | High | Medium | ADR-0006 idempotency + stable paths |
| `UndoTransaction` reverts human edits | Medium | High | Ban agent-facing undo; scoped undo only |
| Local malware/process drives MCP | Medium (loopback) | Critical | Loopback bind; no LAN bind; tiers; audit (auth out of scope without Epic fork) |
| Script tool escapes into OS | Low (restricted imports) | High | `unsafe` off; do not wrap execute_tool_script in UEREMCP v1 |
| Writes to Saved/Config evade sandbox rollback | Medium | Medium | Path policy; never claim rollback for non-mount paths |
| Modal freeze → agent thrash | Medium | Medium | UnrealWatchMCP companion; single retry policy |
| Cross-project write | Low–Medium | Critical | `project.path` must match open project |

---

## Negative findings

1. **No MCP authentication** beyond Origin — confirmed source + runtime.
2. **No public "is modal blocking" API** in MCP/ToolsetRegistry headers (WS-04;
   this run did not find one either).
3. **ADR-0009 does not exist yet** in `docs/adr/` — job model evidence lives in
   WS-04 `RB-04` + `transport_job_handoff.json`. Mutator queue should compose with
   that poll model when ADR-0009 lands.
4. **PIE / unsaved-package gates** not runtime-verified this run (editor MCP
   `list_toolsets` timed out / blocked; only `ping` Origin probes used).
5. **UnrealWatchMCP MCP server** was unavailable/timeout from this agent harness;
   README + WS-04 proposal remain the evidence base.
6. **`docs/SECURITY.md` and `UeremcpSecurity/`** do not exist yet — intentionally
   deferred until Phase 1 gates / ADR-0010 acceptance (this run: research + proposals
   only).

---

## API availability summary

| API / capability | Public | Editor-only | C++ | Python | Notes | Tag |
|---|---|---|---|---|---|---|
| `ValidateOriginHeader` | No (server private) | Yes | Yes | No | Inherited; not extendable without fork | `[VERIFIED: ModelContextProtocolServer.cpp:73]` |
| HTTP bind address | Via HttpServer config | Yes | Yes | No | Default localhost | `[VERIFIED: HttpServerConfig.h:13]` |
| `FGlobalSandbox` | Yes | Yes | Yes | Yes | Transaction only | `[VERIFIED: SandboxLibrary.h]` |
| `ISandboxInstance` mount scope | Yes | Yes | Yes | — | No Saved/Config | `[VERIFIED: ISandboxInstance.h:28-30]` |
| `UToolsetLibrary::UndoTransaction` | Yes | Yes | Yes | Yes | Global stack | `[VERIFIED: ToolsetLibrary.h:129]` |
| `ProgrammaticToolset.execute_tool_script` | Yes (tool) | Yes | — | Yes | Unsafe tier | `[VERIFIED: programmatic.py:906]` |
| UnrealWatchMCP | Host process | N/A | No | Yes | Companion | `[VERIFIED: README.md]` |
| MCP Bearer auth | — | — | — | — | **Absent** | `[VERIFIED-RUNTIME + grep]` |

---

## Architectural implications

- ADR-0002 stands: no external MCP server / no Epic fork for auth. Security is
  **application-layer** in `UeremcpSecurity` + operational bind discipline.
- ADR-0005 stands: sandbox is rollback. WS-11 supplements (DiscardFiles, Leave,
  Saved/Config) are security-relevant hazards — adopt in hazard list.
- ADR-0006 necessary but insufficient for swarm writes — needs mutator queue
  (ADR-0010).
- Envelope `dry_run` default `false` is fine for create/update; ADR-0010 must
  define the destructive override table (schema description already points here).
- Do not implement `UeremcpSecurity` until Wave 2 gates (WS-03 plugin, WS-05
  protocol, WS-11 harness) — research complete.

---

## Open questions

1. Can UEREMCP detect non-loopback HTTP bind from public API without parsing
   `netstat`? (Wave 2 spike)
2. Exact PIE detection API for mutator refusal — needs `[VERIFIED-RUNTIME]`.
3. Should `unsafe` be a project DeveloperSetting or an envelope field? (Recommend
   project setting + per-request cannot elevate.)
4. Audit PII / path redaction for shared logs — policy TBD with owner.
5. Whether Git soft-check belongs in WS-12 or WS-13 docs only for POC.

---

## Deliverables checklist

- [x] Threat model (agent-error centred)
- [x] ADR-0010 recommendation (tiers, defaults, enforcement) → proposal
- [ ] Path-validation implementation in `UeremcpSecurity` — **deferred Wave 2**
- [ ] Audit logging implementation — **deferred Wave 2**
- [x] Hazard list → `docs/proposals/ws-12-hazard-list.md`
- [x] Binding + `bAutoStartServer` recommendation (above + proposal)

## Tests / probes run this session

- `netstat` bind check on :8000/:8001 (non-mutating)
- HTTP `ping` Origin / Authorization probes (non-mutating)
- Header/source reads under `$MCP`, `$TR`, FileSandbox, HttpServer, Epic ProgrammaticToolset
- Read-only inspection of RE `.mcp.json`, MCP settings ini, `.git`
- `python tools/check_ownership.py --ws WS-12` (at commit time)
- No asset mutations; no sandbox Enter; no UndoTransaction
