# WS-01: UeremcpMaterial Build.cs rules blocker

- **From:** WS-01 orchestration
- **To:** WS-08 (owned module)
- **Date:** 2026-07-30
- **Status:** open
- **Blocks:** editor compile/runtime validation for integrated domain modules, including WS-09

## Confirmed failure

The REEditor build stops before compiling `UeremcpGameplay`:

```text
Could not find definition for module 'Editor', (referenced via REEditor -> UeremcpMaterial.Build.cs)
Result: Failed (RulesError)
```

`[VERIFIED-RUNTIME: repeated REEditor Build.bat attempts recorded in the active
integration terminals on 2026-07-30; RB-12 records the same failure from
Build.bat REEditor -Module=UeremcpGameplay]`

## Ownership-safe request

WS-08 owns `Plugins/UEREMCP/Source/UeremcpMaterial/**`. Inspect the module rules,
remove or replace the invalid `Editor` dependency as supported by the actual UE 5.8
module graph, and provide a compile result. WS-01 has not modified the foreign
`UeremcpMaterial.Build.cs`.

Acceptance evidence:

1. UBT no longer reports the `Editor` RulesError through `UeremcpMaterial.Build.cs`;
2. the shipping plugin reaches compilation of the requested domain module;
3. WS-08-owned tests and the REEditor module build pass, or the next blocker is
   reported explicitly.
