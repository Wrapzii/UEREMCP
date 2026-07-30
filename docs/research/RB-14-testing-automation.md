# RB-14: Testing and automation infrastructure

- **Owner:** WS-11
- **Status:** not_started
- **Blocks:** every verification claim in the project
- **Priority:** high

## Framing

`AGENTS.md` rule 6 says success requires verification, and rule 7 says every deliverable
ships with tests. Neither is possible until there is a harness. **This brief blocks the
credibility of every other workstream**, so it runs in Wave 1.

`AutomationTestToolset` is enabled in RE `[VERIFIED: RE.uproject]` — start there.

## Questions

1. What automation test frameworks are available for an editor plugin —
   `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `FAutomationTestBase`, functional tests, Gauntlet?
   Which suit asset-manipulation tests?
2. How are tests run — editor UI, commandlet, `UnrealEditor-Cmd.exe` with
   `-ExecCmds="Automation RunTests ..."`? **Can they run without a human present?** If
   not, the swarm cannot self-verify and that changes how work is accepted.
3. How long does a minimal editor-integration test cycle take end to end? If it is 10
   minutes, agents will skip it; design around that reality rather than mandating
   something nobody runs.
4. Can tests create, mutate, and clean up assets safely in a scratch content path
   (`/Game/__UeremcpTests/`)? How do we guarantee cleanup even on failure — and never
   touch real project content?
5. Can Blueprint compilation, Niagara compilation, and material/shader compilation be
   awaited deterministically in a test? Async compilation is the most likely source of
   flaky tests here.
6. Can PIE be driven from a test for runtime smoke tests — spawn an actor, activate an
   ability, confirm a Niagara system actually emitted particles? What does it cost, and
   is it stable enough to gate on?
7. Does `AutomationTestToolset` let an **agent** run tests through MCP? If so, an agent
   can verify its own work in the same session — a significant capability, and it should
   be wired into the response `validation` block rather than left as a separate manual
   step.
8. How do we test the **rollback** path, given ADR-0005 gates every rollback claim on
   `Rollback.MultiAssetDiscard`? Coordinate with RB-06; this test is jointly owned.
9. How do we test **idempotency** (`Idempotency.RepeatedCreate`) and **revision
   conflict** (`Revision.StaleRejected`) from ADR-0006?
10. Can unit tests for pure logic — schema validation, graph serialisation, patch
    application, `$ref` resolution, hashing — run **outside** the editor for fast
    iteration? Strongly preferred; it is the only way tests get run often.
11. What does the benchmark harness need to measure the metrics in ADR-0003 —
    `mcp_round_trips`, `internal_operations`, tokens, and **completion rate**? Extend
    REAgentTools' existing A/B harness (`$RAT/Docs/BENCHMARK_REPORT.md`,
    `benchmark_ab_live.json`) so numbers stay comparable to the ~5:1 baseline
    (`docs/WHY.md`). Do not start a fresh benchmark that cannot be compared.
12. Can the editor be launched and torn down repeatedly in an automated loop, and how
    reliably? This determines whether "survives editor restart" (POC E) is testable.

## Deliverables

- [ ] A working test harness with one passing editor integration test — **Wave 1
      blocker for everyone**
- [ ] A fast out-of-editor unit test path for pure logic
- [ ] `Rollback.MultiAssetDiscard`, `Idempotency.RepeatedCreate`,
      `Revision.StaleRejected` implemented
- [ ] Scratch-path conventions and guaranteed-cleanup helpers, published to all
      workstreams
- [ ] A benchmark harness extending REAgentTools', reporting calls / tokens /
      **completion rate**
- [ ] A written statement of what **cannot** be automatically tested, so nobody claims
      verification they did not perform
