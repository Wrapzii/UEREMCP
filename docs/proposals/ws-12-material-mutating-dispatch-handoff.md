# WS-12 → WS-08: Wire `FUeremcpMutatingDispatch` on Material mutate paths

- **From:** WS-12 (Security & Reliability)
- **To:** WS-08 (`Plugins/UEREMCP/Source/UeremcpMaterial/**`)
- **Date:** 2026-07-30
- **Status:** Open — WS-12 cannot edit Material paths
- **Related:** ADR-0010, R-07, `docs/SECURITY.md`,
  `docs/proposals/ws-12-niagara-mutating-dispatch-handoff.md` (same pattern)

## Why

Acceptance audit: Material mutators create/update material instances and
procedural textures without the shared ADR-0010 gate
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialToolset.cpp
CreateVfxMaterial / CreateProceduralTexture]`.

Gameplay and Blueprint already use `FUeremcpMutatingDispatch`. Niagara has a twin
handoff. Security is a policy library — **no** `RegisterToolsetClass`.

## Exact call sites

### 1. Build dependency

**File:** `Plugins/UEREMCP/Source/UeremcpMaterial/UeremcpMaterial.Build.cs`

Add `"UeremcpSecurity"` to `PrivateDependencyModuleNames` (module already depends
on `"UeremcpCore"`).

### 2. `CreateVfxMaterial`

**File:** `Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialToolset.cpp`  
**Function:** `UUeremcpMaterialToolset::CreateVfxMaterial`

After action / `target.asset_path` validation and **before**
`UeremcpMaterialService::ExecuteCreateVfxMaterial(Request)`:

```cpp
#include "UeremcpMutatingDispatch.h"
#include "UeremcpSecurityDomainAdoption.h"

const bool bTargetExists = /* asset exists at Request.TargetAssetPath */;
const bool bDestructiveMode =
	Request.Mode.Equals(TEXT("replace"), ESearchCase::IgnoreCase)
	|| Request.Mode.Equals(TEXT("rebuild_from_specification"), ESearchCase::IgnoreCase)
	|| Request.Mode.Equals(TEXT("delete"), ESearchCase::IgnoreCase);
const int32 PredictedDeleted =
	FUeremcpSecurityDomainAdoption::PredictedDeletedForDestructiveReplace(
		bTargetExists, bDestructiveMode);

FUeremcpMutatingDispatch MutatingDispatch;
FString BlockingResponse;
if (!Request.bDryRun)
{
	if (!MutatingDispatch.TryBegin(
		RequestJson,
		bTargetExists,
		PredictedDeleted,
		false,
		BlockingResponse))
	{
		return BlockingResponse;
	}
}

const FUeremcpMaterialCreateResult CreateResult =
	UeremcpMaterialService::ExecuteCreateVfxMaterial(Request);

// ... build FUeremcpResponse as today ...

return Request.bDryRun
	? FUeremcpEnvelope::SerializeResponse(Response)
	: MutatingDispatch.Complete(Response);
```

If `ExecuteCreateVfxMaterial` honours `Request.bDryRun` internally, keep that
behaviour; the gate's `IsEffectiveDryRun()` should drive any additional force
from policy when you stop skipping `TryBegin` on dry-run.

### 3. `CreateProceduralTexture`

**Same file:** `UUeremcpMaterialToolset::CreateProceduralTexture`

Identical gate around `UeremcpProceduralTextureService::ExecuteFromEnvelope`.
Texture writes under `/Game/__UeremcpTests/Textures/` still must pass
`FUeremcpPathPolicy` via the dispatch (do not loosen domain allowlists).

### 4. Echo

Leave ungated.

## Expected tests (WS-08 owns)

| Test | Assertion |
|---|---|
| `create_vfx_material` target `/Engine/...` | rejected; service not entered |
| Soft-path traversal `/Game/../Secret` | rejected |
| `mode=replace` + existing MI, omit `dry_run` | policy-forced dry-run or no mutation |
| Concurrent live creates | second `partially_completed` + job id |
| Contract grep | `CreateVfxMaterial` and `CreateProceduralTexture` both call `MutatingDispatch.TryBegin` / `Complete` |
| Existing dry_run plan/toolset tests | still green |

## Reference

- Gameplay: `UeremcpGameplayToolset.cpp` + `FUeremcpMutatingDispatch`
- Docs: `docs/SECURITY.md` § Preferred domain gate
- Helpers: `FUeremcpSecurityDomainAdoption`

## Acceptance for R-07 (Material slice)

Both live mutators gated; path rejection before service; audit on terminal live
responses; ownership check clean for WS-08.

R-07 stays open until Niagara **and** Material land.
