# RB-01: Engine baseline, source availability, and API stability

- **Owner:** WS-01
- **Status:** not_started
- **Blocks:** ADR-0001 confidence, everyone's ability to verify claims
- **Priority:** high

## Framing

ADR-0001 commits to UE 5.8 and to Epic's Experimental agent stack. Two practical
questions determine how much the rest of the project can actually *verify* rather than
infer, and how much churn risk we carry.

## Questions

1. What exact UE 5.8 build is installed — version, changelist, GA or Preview? Check
   `$ENGINE/Build/Build.version` and the launcher.
2. **Is engine `.cpp` source available, or only installed headers?** This bounds every
   other brief: with source, behaviour can be `[VERIFIED]`; without it, behaviour must
   be `[VERIFIED-RUNTIME]` or stays `[UNVERIFIED]`. Check whether
   `$MCP/Source/*/Private/*.cpp` and `$TR/Source/ToolsetRegistry/Private/*.cpp` exist,
   or only `Public/*.h`.
3. Is a GitHub engine-source clone available to the project as a reference, even if the
   installed build is binary?
4. What is the hotfix cadence, and what is the upgrade plan? All three plugins we depend
   on are `IsExperimentalVersion: true`, and two are `NoRedist`
   `[VERIFIED: .uplugin files]`.
5. Are there release notes or docs for `ModelContextProtocol` / `ToolsetRegistry`? They
   are undocumented in the local install as far as recon showed — confirm.
6. Is UE 5.9 announced, and does it change this stack? Committing hard to Experimental
   APIs that are about to be replaced is a real risk.
7. What does `$PROJ/Refs/` contain? It may already hold engine reference material.
8. Does the RE project build from source today, and how long does a full and an
   incremental build take? This sets everyone's iteration budget and is worth knowing
   before promising a schedule.
9. Are there existing `.codex`, `.cursor`, or `.claude` configurations in `$PROJ` and
   `$RAT` that encode conventions the swarm should follow? `$PROJ/AGENTS.md` and
   `$PROJ/AGENT_SYNC.md` (221 KB) exist — summarise anything binding.

## Deliverables

- [ ] Exact engine version and build recorded in `GROUNDED_FACTS.md` (WS-01 edits)
- [ ] A clear statement of whether engine `.cpp` is readable — **broadcast this to
      every workstream**, it changes what verification tags they can honestly use
- [ ] Risk-register updates for API churn
- [ ] Build-time measurements
- [ ] A summary of binding conventions from existing project agent configs
