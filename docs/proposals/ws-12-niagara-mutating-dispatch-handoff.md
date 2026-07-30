# WS-12 → WS-07: Wire `FUeremcpMutatingDispatch` on Niagara mutate paths

- **From:** WS-12 (Security & Reliability)
- **To:** WS-07 (`Plugins/UEREMCP/Source/UeremcpNiagara/**`)
- **Date:** 2026-07-30
- **Status:** Open — WS-12 cannot edit Niagara paths
- **Related:** ADR-0010, R-07, `docs/SECURITY.md`,
  `docs/proposals/ws-12-core-security-dispatcher-gate.md`

## Why

Acceptance audit: ADR-0010 security is applied for Blueprint + Gameplay via
`FUeremcpMutatingDispatch`, but **not** for Niagara. Live
`create_niagara_effect` can create/replace probe assets without permission,
path, mutator-queue, or audit gates
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraToolset.cpp
CreateNiagaraEffect]`.

`UeremcpSecurity` intentionally has **no** `RegisterToolsetClass` — it is not a
toolset. Domains call Core's `FUeremcpMutatingDispatch`, which composes Security
primitives `[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Public/UeremcpMutatingDispatch.h]`.

R-07 remains **open** until this handoff lands (and Material's twin).

## Exact call sites

### 1. Build dependency

**File:** `Plugins/UEREMCP/Source/UeremcpNiagara/UeremcpNiagara.Build.cs`

Add `"UeremcpSecurity"` to `PrivateDependencyModuleNames` (alongside existing
`"UeremcpCore"`). `MutatingDispatch.h` includes `UeremcpSecurityTypes.h`.

### 2. Mutating tool entry

**File:** `Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraToolset.cpp`  
**Function:** `UUeremcpNiagaraToolset::CreateNiagaraEffect`

Today the handler parses the envelope, validates probe path, then calls
`FUeremcpNiagaraCreate::Run` and serializes with `FUeremcpEnvelope::SerializeResponse`
with **no** security gate.

**Required shape** (mirror Gameplay `CreateSpell` /
`docs/proposals/ws-06-mutating-dispatch-adoption.md`):

```cpp
#include "UeremcpMutatingDispatch.h"
#include "UeremcpSecurityDomainAdoption.h"  // optional PredictedDeleted helper

// After probe-path / spec parse success, before FUeremcpNiagaraCreate::Run:

const bool bTargetExists = /* LoadPackage / asset registry check for Request.TargetAssetPath */;
const bool bReplaceMode = /* same predicate as UeremcpNiagaraProbeAssets::IsReplaceMode(Request.Mode) */;
const int32 PredictedDeleted =
	FUeremcpSecurityDomainAdoption::PredictedDeletedForDestructiveReplace(
		bTargetExists, bReplaceMode);

FUeremcpMutatingDispatch MutatingDispatch;
FString BlockingResponse;
if (!Request.bDryRun)  // OR always TryBegin — prefer always if dry-run still needs audit
{
	if (!MutatingDispatch.TryBegin(
		RequestJson,
		bTargetExists,
		PredictedDeleted,
		/*bReadOnlyOperation=*/false,
		BlockingResponse))
	{
		return BlockingResponse;
	}
}

// ... existing Create::Run + round-trip ...

return Request.bDryRun
	? FUeremcpEnvelope::SerializeResponse(Response)
	: MutatingDispatch.Complete(Response);
```

**Do not** skip `TryBegin` on `mode=replace` when the target exists and
`options.dry_run` is absent — permission will force effective dry-run; if you
skip the gate entirely, that force never runs.

### 3. Read-only tools (optional but recommended)

**Function:** `UUeremcpNiagaraToolset::InspectSystem`

Call `TryBegin(..., bReadOnlyOperation=true)` then `Complete` so path/project
mismatch and audit stay consistent. Queue is bypassed for read tier.

`Echo` may remain ungated (no state touch).

## Expected tests (WS-07 owns)

| Test | Assertion |
|---|---|
| Editor: create under `/Engine/...` | `status=rejected`, reason from path policy |
| Editor: `mode=replace` + existing target, **omit** `options.dry_run` | Effective dry-run (no delete/create) **or** rejection if domain requires explicit false — must not silently destroy |
| Editor: two concurrent live creates same project | Second returns `partially_completed` + job id (mutator queue) |
| Automation / contract: include `UeremcpMutatingDispatch.h` and a `TryBegin` call site in `CreateNiagaraEffect` | Static grep test (pattern like Gameplay `schemas/domains/gameplay/test_specifications.py`) |
| Existing dry_run create tests | Still pass; prefer `Complete`/serialize consistency |

Core/Security already cover policy unit tests under `UEREMCP.Security.*` and
`UeremcpCore.MutatingDispatch.*` — do **not** reimplement those in Niagara.

## Reference adopters

- `Plugins/UEREMCP/Source/UeremcpGameplay/Private/UeremcpGameplayToolset.cpp`
  (`CreateSpell` ~187–200, `Complete` ~481)
- `Plugins/UEREMCP/Source/UeremcpBlueprint/Public/UeremcpBlueprintMutatingGate.h`

## Acceptance for R-07 (Niagara slice)

- Live `CreateNiagaraEffect` always goes through `TryBegin` / `Complete` when not
  dry-run (and preferably always).
- Disallowed soft roots rejected before `FUeremcpNiagaraCreate::Run`.
- Audit JSONL line written on terminal live outcomes.
- `python tools/check_ownership.py --ws WS-07` clean.

WS-12 will not mark R-07 closed from Security side alone.
