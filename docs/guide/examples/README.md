# Published request examples

These JSON files are canonical **request shapes**. They are not a guarantee that
the sample `target.asset_path` exists in every RE checkout or editor session.

For read, inspect, or visual-capture examples:

1. Use `editor_toolset.toolsets.asset.AssetTools.find_assets` for read-only
   discovery under `/Game/__UeremcpTests/` or `/Game/__UeremcpPoc/`.
2. Replace the example `target.asset_path` with a discovered scratch asset.
3. Keep the routed UEREMCP semantic tool from
   [`../tool-selection-policy.md`](../tool-selection-policy.md).

A missing target should produce an honest `rejected` response. Do not respond by
falling back to Blueprint pin loops, Niagara module primitives, or screenshot
authoring.

Create examples intentionally name scratch targets. Review `mode`, `dry_run`,
`expected_revision`, and `idempotency_key` before changing a request from example
to execution.
