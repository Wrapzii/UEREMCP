# Capability Matrix — <system name>

- **Owner:** WS-02
- **Status:** not_started | in_progress | complete
- **Source:** how the inventory was obtained (runtime enumeration / source read / docs)
- **Last verified:** YYYY-MM-DD

## Purpose

Master prompt §12: the new system must at minimum match the practical capability
coverage of what exists, though **not** its low-level tool interface. A capability may
legitimately be provided by a superior goal-level operation — fifty primitive Blueprint
node tools may be replaced by graph retrieval, graph replacement, semantic patching,
validation, and repair.

This matrix is how we prove coverage rather than assume it, and how we avoid rebuilding
things that already work (R-06).

## Disposition values

| Value | Meaning |
|---|---|
| `preserve` | Keep as-is and expose it. Works, and is at the right altitude. |
| `internalise` | Keep, but hide from agents via `SetNameFilters`. Becomes an internal primitive our goal-level tools compose. |
| `supersede` | Replaced by a named UEREMCP action that covers it better. Coverage must be equal or greater. |
| `retire` | Drop. Requires a stated reason it is not needed. |
| `defer` | Not yet decided. Must name what decides it. |

## Matrix

One row per **tool**, not per toolset.

| Toolset | Tool | Purpose | Input | Output | Limitations | Altitude | Disposition | Superseded by | Tag |
|---|---|---|---|---|---|---|---|---|---|
| | | | | | | primitive / composite / goal | | | |

Column notes:

- **Limitations** — what it cannot do, or does badly. This is the column that justifies
  building anything new. A blank limitations cell with a `supersede` disposition is not
  acceptable.
- **Altitude** — primitive (one editor operation), composite (several), goal (a complete
  outcome). Anything already at `goal` altitude is a strong `preserve`.
- **Tag** — verification tag per `docs/RESEARCH_PROTOCOL.md`. A row derived from
  someone's documentation rather than observed behaviour is `[UNVERIFIED]` and must be
  labelled so.

## Do-not-rebuild list

The subset of tools already at goal altitude, or good enough that duplicating them would
be waste. **Every domain workstream reads this before writing a tool.**

## Real gaps

Capabilities with no existing equivalent anywhere. This is where new work is justified
without further argument.

## Coverage assertion

Once dispositions are assigned, state plainly: does the planned UEREMCP surface cover
every `supersede` row? List any coverage that is claimed but not yet built, so §12 is
not quietly failed.
