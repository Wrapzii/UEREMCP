# UEREMCP content_hash semantics

**Owner:** WS-05  
**Status:** Accepted for protocol v1 (ADR-0004 / ADR-0006 open question closed here)  
**Format:** `sha256:<lowercase-hex>` per `schemas/common/defs.schema.json#/$defs/contentHash`

## Goal

`content_hash` (and currently `revision`, which equals it) must be:

- **Stable** across cosmetic churn: node reordering, position jitter, GUID /
  `node_id` / `pin_id` regeneration, `retrieved_at` changes.
- **Sensitive** to semantic change: pin defaults, connections, node properties /
  defaults, node class / semantic type, variables, and other non-cosmetic graph
  structure.

Getting this wrong makes ADR-0006 conflict detection either useless (hash always
changes) or maddening (hash misses real edits).

## Algorithm

1. Parse the graph JSON.
2. **Drop ignored fields** wherever they appear:
   - `position`, `bounds`
   - `content_hash`, `revision`, `retrieved_at`
   - `node_id`, `pin_id`, `comment_id`
   - keys named `guid`, `*_guid`, `node_guid`, `pin_guid` (case-insensitive)
3. **Stable node identity** for sorting and link endpoints:
   - Prefer `semantic_id` → key `sem:<semantic_id>`
   - Else derive `cls:<node_class>|type:<semantic_type>|title:<title>`
4. **Pins** are identified by `direction:name` (not `pin_id`).
5. **Top-level `links[]`** are rewritten to stable node keys + stable pin names,
   then sorted lexicographically by
   `from_node>from_pin>to_node>to_pin>kind`. Pin-level `links` arrays are
   **omitted** from the hash (they duplicate the edge list and embed
   retrieval-local ids).
6. **`nodes[]`** are canonicalised (pins sorted by `direction|name`), then sorted
   by `_stable_key`.
7. All JSON objects emit keys in sorted lexicographic order; arrays keep their
   post-canonicalisation order; condensed separators `,` / `:`.
8. SHA-256 over the UTF-8 bytes of that canonical JSON; prefix with `sha256:`.

## What this deliberately does NOT hash

- Layout: `position`, comment `bounds`
- Engine identity: GUIDs, retrieval-local `node_id` / `pin_id`
- Retrieval metadata: `retrieved_at`, prior `content_hash` / `revision`

## Coordination with WS-06 (RB-05 q14)

Domain retrieve code should populate `semantic_id` whenever a stable role-derived
id is available. Without it, two rebuilt nodes that share class/type/title can
collide in the stable key — domains must document that limitation in
`fidelity.lossy_areas` rather than silently accepting hash instability.

Compile nondeterminism (RB-05 q15) is outside this layer: hashing operates on the
structured JSON representation, not on compiled bytecode.

## Implementation

| Language | Location |
|---|---|
| C++ (production) | `FUeremcpContentHash` in this module |
| Python (regression / golden generator) | `Tests/py/ueremcp_protocol/content_hash.py` |
| Shared vectors | `Tests/golden/content_hash/` |

Cross-language contract is the golden `hash.expected.txt`, asserted by Python
`test_golden.py` and C++ `UEREMCP.Protocol.Golden.ContentHash`. **Do not claim
byte-identical C++/Python output until that AutomationTest passes** — see
`Docs/CPP_PARITY.md` (WS-14 C-2).
