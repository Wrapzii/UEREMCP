# WS-03 → WS-05: mechanical UE 5.8 FJsonObject key-type fixes in Protocol

- **From:** WS-03
- **To:** WS-05
- **Date:** 2026-07-29
- **Why:** Plugin load fails — `UeremcpProtocol` could not be found (no DLL). Shipping gate.

## Cause

UE 5.8 defaults `FJsonObject::Values` to `TMap<UE::FSharedString, ...>`
(`[VERIFIED: Engine/.../JsonObject.h]` — `UE_JSONOBJECT_LEGACY_STRING_KEYS=0`).
Protocol code assumed `FString` keys (`GetKeys(TArray<FString>)`,
`TSet<FString>::Contains(Pair.Key)`). That does not compile. Setting
`UE_JSONOBJECT_LEGACY_STRING_KEYS=1` only in Protocol.Build.cs is **unsafe**
(ABI mismatch with Engine Json).

## Action taken by WS-03 (mechanical, no semantic change)

Edited Protocol sources only to convert `Pair.Key` / key arrays via `FString(...)`
and add `#include "Policies/CondensedJsonPrintPolicy.h"`. Logic unchanged.
Please adopt or replace on `ws-05-protocol`.

## Response (WS-01)

**Accepted.** Integrated onto `ws-01-orch` (`56f5d36`) by taking the WS-03
versions of the conflicted Protocol `.cpp` files. Do **not** set
`UE_JSONOBJECT_LEGACY_STRING_KEYS=1` in Protocol.Build.cs. WS-05: rebase
`ws-05-protocol` onto orch (or cherry-pick the key conversions) so the next
Protocol edit does not regress the compile.
