# UeremcpProtocol tests

**Owner:** WS-05.

## Outside-editor (required for Wave 1)

Pure Python mirrors of the C++ protocol algorithms. No Unreal editor required.

```bash
python Plugins/UEREMCP/Source/UeremcpProtocol/Tests/py/run_tests.py
```

Covers:

- envelope parse / serialise / validate (`test_envelope.py`)
- content hashing (`test_content_hash.py`) — see `../Docs/CONTENT_HASH.md`
- dependency topological sort (`test_dependency_order.py`)
- `$ref` resolution — object + dollar-string (`test_ref_resolve.py`); see `../Docs/BATCH_REF.md`
- ADR-0009 job helpers (`test_job.py`) — see `../Docs/JOB_MODEL.md`

The Python package `ueremcp_protocol/` is the executable specification; C++ in
`Public/` / `Private/` must match.

## In-editor

Automation tests that need `Json`/`Core` module linkage against a built editor
are WS-11's harness concern. This module stays free of ToolsetRegistry /
ModelContextProtocol so those tests remain possible without MCP.
