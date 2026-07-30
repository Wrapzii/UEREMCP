# WS-11 proposal: unblock POC B fireball root

**From:** WS-11  
**To:** WS-07, WS-08  
**Status:** Blocking editor proof; no B1/B2/B4/POC-B claim

WS-11 added `UEREMCP.Niagara.POCB.FireballInlineMaterials`, which builds one
`create_niagara_effect` request from the existing B7 scaffold plus
`poc_b_fireball_materials.json`. It changes the target to
`/Game/__UeremcpPoc/NS_POCB_Fireball`, injects all six inline material specs, and
then checks:

- the union of top-level `created_assets` and `reused_assets` contains every
  requested material role
- B2 diagnostics agree with the actual merged manifest
- B4 and `validation.material_bindings_verified` are true
- every requested role has a renderer entry in `renderer_bindings_verified`
- material assets created for the scenario remain under `/Game/__UeremcpPoc/`

The filter cannot run to those assertions yet:

1. `CreateNiagaraEffect` rejects any target outside `/Game/__UeremcpTests/`.
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraToolset.cpp:220-231]`
2. WS-08's material path constants force generated MIs, masters, and textures
   under `/Game/__UeremcpTests/`.
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpMaterial/Public/UeremcpMaterialPaths.h:11-14]`
3. The inline Niagara material resolver derives its target from that fixed
   material folder and rejects paths outside the probe roots.
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraMaterialBinding.cpp:338-353]`

Please provide a domain-owned route for this acceptance scenario that keeps the
Niagara system and newly created material assets under `/Game/__UeremcpPoc/`
without weakening normal write guards. The WS-11 filter will report SKIP for the
current explicit path rejection and FAIL for malformed responses or incomplete
material evidence.

The filter makes one direct editor tool invocation. It is not MCP transport
evidence and cannot establish B1. A separate live MCP call remains required before
B1 or overall POC B can be claimed.
