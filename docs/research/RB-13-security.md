# RB-13: Security, permissions, and reliability boundaries

- **Owner:** WS-12
- **Status:** not_started
- **Blocks:** ADR-0010
- **Priority:** high

## Framing

This system will have write access to a real project the owner has been building, and
will be driven by a swarm of agents running unattended. The realistic threat is not an
attacker — it is **an agent confidently destroying work**, at speed, in parallel.

Design accordingly. The controls that matter most are the boring ones: allowed roots,
dry-run defaults, and an audit trail good enough to reconstruct what happened.

## Questions

### A. What is exposed today

1. Does Epic's MCP server **authenticate at all**? Assume not until proven
   `[GROUNDED_FACTS.md §7.3]`. If it does not, anything on the machine — or on the
   network, if it binds beyond loopback — can drive the editor.
2. What interface does it bind to? `127.0.0.1` only, or `0.0.0.0`? Verify empirically,
   do not infer from the settings names.
3. What can existing Epic toolsets already do — arbitrary Python execution, console
   commands, file writes, OS commands? `ProgrammaticToolset.execute_tool_script` (if it
   exists, RB-02 q7) is effectively arbitrary code execution. Enumerate the blast radius
   we inherit rather than create.
4. What does `FGlobalSandbox` / `FileSandbox` restrict, and is it a **security** boundary
   or only a **transaction** boundary? Almost certainly the latter — confirm, and do not
   let anyone treat it as the former.

### B. What we must add

5. **Allowed project and asset roots.** How do we enforce that operations stay inside
   `/Game/...` and the project directory, and reject traversal, absolute paths, and
   `/Engine/` writes?
6. **Permission tiers.** Propose the tier set for ADR-0010. Suggested starting point —
   refine it: `read` (inspection only) · `write` (create/modify inside allowed roots) ·
   `destructive` (delete, replace user content) · `unsafe` (arbitrary script, console,
   OS). Default should be `write`, with `destructive` requiring explicit opt-in per
   request and `unsafe` off by default.
7. **Destructive defaults.** ADR-0003 gives `dry_run` a default of `false`. Which
   actions must flip that to `true`? At minimum `delete`, and `replace` when it targets
   existing user content.
8. **Audit trail.** What is logged, where, and is it enough to answer "what did the
   agents change in the last hour, and can I undo it?" This is the single most valuable
   control for the owner's actual risk.
9. **Source control.** Is SCC configured in RE? If so, should agent operations check
   out files, and can SCC serve as the real safety net that makes rollback bugs
   survivable? Coordinate with RB-06.
10. **Concurrency hazards.** Multiple agents writing the same asset, the same gameplay
    tag table (RB-12 q5), or the same config. ADR-0006 handles the optimistic-concurrency
    case; what needs an actual lock?
11. **Editor state hazards.** Operating during PIE, with unsaved user changes, or with a
    modal dialog open. `Optional/UnrealWatchMCP` in REAgentTools exists because modal
    dialogs hang tool calls — evaluate it (RB-04 q17).
12. **`UndoTransaction` hazard.** Could a tool call undo *user* work rather than agent
    work? RB-06 q12 flags this. Treat it as a destroy-user-work bug class, not a
    correctness nicety.

## Deliverables

- [ ] A written threat model centred on agent error, not external attackers
- [ ] A recommendation for ADR-0010: tiers, defaults, and enforcement points
- [ ] Path-validation implementation in `UeremcpSecurity`
- [ ] Audit logging that answers "what changed in the last hour"
- [ ] A hazard list published to all workstreams — especially anything that can silently
      destroy user content (`AGENTS.md` rule 8)
- [ ] A recommendation on binding, and on whether `bAutoStartServer` should be enabled
      by default
