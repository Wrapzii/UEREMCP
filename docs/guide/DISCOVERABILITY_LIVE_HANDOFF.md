# Discoverability live-validation handoff

Static/unit/BuildPlugin work is complete on `ws-03-discoverability-backlog`.
No editor was started and the RE plugin junction was not changed while the
environment validation sibling was running.

## Why live validation is still required

`tools/registry_snapshot.json` is intentionally fail-closed and stale: it does
not contain every `AICallable` declared by the current source, including the
router bootstrap tools. Do not hand-edit or synthesize the snapshot. The dump
tool now refuses to overwrite ground truth when the running editor is missing a
source-declared callable.

## Exact live checks

1. Deploy the BuildPlugin output or rebuild the plugin used by RE. Confirm the
   RE junction points to the intended deployment only after the environment
   sibling has finished.
2. Close every editor instance, then start exactly one RE editor owning MCP
   port 8000. This avoids the stale-session collision observed on 2026-07-30.
3. Run:

   ```powershell
   python tools/dump_tool_registry.py
   python tools/check_tool_names.py
   python tools/gen_focus_config.py --check
   python -m unittest tests.intent_router.test_intent_router_contract
   ```

   `dump_tool_registry.py` must exit 0 and record all 35 current source
   callables plus `source_surface_fingerprint`. `check_tool_names.py` must report
   zero stale-snapshot, unknown-tool, description, and domain problems.
4. Through MCP, verify
   `UeremcpCore.UeremcpReferenceToolset` contains exactly one each of
   `GetStarted`, `ResolveIntent`, and `DescribeOperation`. No second
   `ResolveIntent` toolset may be present.
5. Call `DescribeOperation` with both `CreateNiagaraEffect` and
   `create_niagara_effect`. Both must return canonical
   `UeremcpNiagara.UeremcpNiagaraToolset.CreateNiagaraEffect`; the alias response
   must include `normalized_from`.
6. Send one malformed or incomplete envelope to any UEREMCP tool. The single
   rejection must contain status `rejected`, the original reason, required
   top-level fields, a complete minimal request shape, and the next
   GetStarted/ResolveIntent action.
7. Inspect every UEREMCP tool returned by `describe_toolset`: each callable must
   expose task vocabulary, explicit inputs/required `specification` keys, and a
   worked request example.
8. Run the held-out router evaluation against the fresh snapshot:

   ```powershell
   python tools/intent_router/router.py --eval-heldout
   ```

   Record top-1, top-3, MRR, confident-wrong count, and abstention accuracy.
   Routing accuracy is not end-to-end completion.

## Focus-mode decision

Do not add `DefaultEditorPerProjectUserSettings.ini` with global
`BlockedNames`. The router demotes superseded primitives while retaining safe
Epic discovery (logs, assets, scene/object inspection, Niagara info/assets,
material instances, capture fallback, and ProgrammaticToolset recovery).
`gen_focus_config.py --write` therefore runs all gates and then refuses the
unsafe global write.
