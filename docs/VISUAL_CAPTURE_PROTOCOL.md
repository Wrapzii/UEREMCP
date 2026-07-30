# Visual Capture Protocol — deterministic VFX / animation verification

**Status:** Draft. Rig validated live on 2026-07-30 against UE 5.8.0
(`5.8.0-55116800+++UE5+Release-5.8`), project `visualtest`. The simulation recipe
is transcribed from the operator's proven
`visualtest/Scripts/capture_ice_wall_baseline0.py`, which predates this document
and has been capturing deterministic Niagara ages successfully; the MCP tool
wrapping it is **not yet compiled or run** (§9).
**Owner:** WS-11 (cross-domain verification infrastructure).
**Related:** [`WHY.md`](WHY.md) (cost model), [`POC_ACCEPTANCE.md`](POC_ACCEPTANCE.md)
criterion B10, RB-07 q17, RB-14.

---

## 1. The problem

An agent changes a Niagara system or a montage and then has to answer *"did that
work?"*. Structured inspection answers *"is the graph what I asked for"* — it does
not answer *"does it look right"*. So agents fall back on screenshots, and hit
three walls:

1. **The subject is moving.** A single screenshot of a 1.5-second effect samples
   one arbitrary instant. Re-running gives a different instant. Nothing is
   comparable across runs.
2. **The subject is unframed.** The agent spawns the effect somewhere, guesses a
   camera position, and usually captures floor, void, or the inside of a wall.
   Each guess costs a round trip.
3. **Nothing is measured.** An image tells the agent nothing it can assert on. A
   test that ends in "the agent looked at a picture" is not a gate.

Each wall is paid for in *tool calls*, and per [`WHY.md`](WHY.md) tool calls are
the superlinear term (`total ≈ N·C₀ + r·N²/2`). Guess-camera → capture → look →
re-aim → capture is exactly the primitive loop this project exists to delete.

## 2. The shape of the fix

A **fixed capture stage**: authored once, reused for every effect.

- One scratch level, `/Game/__UeremcpTests/VisualStage/L_VfxCaptureStage`.
- A known subject anchor. The effect spawns *there*; the camera never hunts for it.
- **Preset cameras**, authored once (front / three-quarter / side / top).
- **Deterministic time stepping**, so frame *i* means the same simulation age on
  every run, on every machine.
- **Output: a frame series plus a numeric signal.** The numbers are the gate; the
  images are for a human or for an agent's qualitative read.

The last point is not new policy — it is [`POC_ACCEPTANCE.md`](POC_ACCEPTANCE.md)
B10 already: *screenshot as supplementary evidence only, never as the validation
itself*. This document is that principle turned into reusable infrastructure
instead of one-off code for one fireball.

## 3. What was validated live

Everything in this section was executed against a running editor, not reasoned
about. Evidence is the observed value.

| Capability | Result | Evidence |
|---|---|---|
| Arbitrary editor Python over Remote Control `:30010` | **works** | `ExecutePythonCommandEx` on `/Script/PythonScriptPlugin.Default__PythonScriptLibrary` |
| Stage build (backdrop, lights, cameras) from script | **works** | 5 backdrop planes + 4 lights spawned and verified by read-back |
| `SceneCaptureComponent2D` → `export_render_target` → PNG | **works** | 640×360 RGBA8 PNGs on disk |
| Capture responds to scene content | **works** | control sphere moved `mean_lum` 118.79 → 128.15, `lit_px` 118957 → 127405 |
| Programmatic per-frame pixel signal | **works** | mean luminance / lit-pixel count / max luminance computed per frame in-editor |
| Deterministic Niagara time stepping | **proven, pre-existing** | `Scripts/capture_ice_wall_baseline0.py` — `set_force_solo` + `reinitialize_system` + `advance_simulation` |
| **Niagara particles in a capture from _this_ harness** | **not yet** | my harness omitted `set_force_solo`; corrected but not re-run (editor unavailable) |

So the rig, the measurement loop, and the simulation recipe are all real and
individually proven. What has not happened yet is one clean run of the corrected
harness end to end.

**Prior art matters here.** `Scripts/capture_ice_wall_baseline0.py` and
`Scripts/assemble_ice_wall_baseline0.py` already did this job, and I did not find
them — I searched `Content/Python/` and never looked in `Scripts/`. Most of the
"landmines" in §4 were rediscoveries of things that script had already solved.
The lesson is the project's own rule 2: audit before you build. A pointer to
`Scripts/` belongs in the repo's read order.

## 4. Landmines found by doing it

Each of these silently produces a plausible-looking wrong answer. Together they
are the reason this document exists before the automation does — an automated
harness written without them would have reported confident nonsense.

**4.1 `create_render_target2d` six-arg form captures pure black, forever, with no
error.**
```python
# BLACK, silently:
rt = unreal.RenderingLibrary.create_render_target2d(
        world, 640, 360, unreal.TextureRenderTargetFormat.RTF_RGBA8,
        unreal.LinearColor(0,0,0,1), False)
# Works:
rt = unreal.RenderingLibrary.create_render_target2d(
        world, 640, 360, unreal.TextureRenderTargetFormat.RTF_RGBA8)
```
Cost me an entire false conclusion: I read "every frame is identical and dark"
as "the effect does not render", when the render target was never valid. Any
harness **must** assert the baseline frame is non-black before trusting a single
downstream comparison.

**4.2 A single `CaptureScene()` can return a stale or empty frame.**
Spawn-then-capture inside one script yields a frame that does not contain the
thing just spawned. The working fix, from the proven recipe, is **clear the
render target and then call `CaptureScene()` three times** before exporting:

```python
unreal.RenderingLibrary.clear_render_target2d(world, target, unreal.LinearColor(0.005, 0.008, 0.015, 1.0))
for _ in range(3):
    capture.capture_scene()
unreal.RenderingLibrary.export_render_target(world, target, folder, name)
```

> **Correction.** An earlier revision of this document claimed actors spawned in
> RC call *N* are not renderable until call *N+1*, and prescribed splitting the
> harness across calls. That was wrong — it was a *workaround* that happened to
> work because extra calls incidentally produced extra rendered frames. The
> operator-proven script does spawn, simulate, capture and export in a single
> pass. Repeat-capture is the actual mechanism; cross-call phasing is not
> required and should not be designed around.

**4.3 Default render-target format is float; `.png` gets EXR bytes.**
`create_render_target2d` defaults to RGBA16f, and `export_render_target` then
writes OpenEXR data under whatever filename you gave it. The files had a `.png`
extension and magic bytes `76 2f 31 01`. Pass `RTF_RGBA8` explicitly.

**4.4 Tracebacks are returned in `CommandResult`, not `LogOutput`.**
An RC client reading only `LogOutput` sees an empty response on every failure and
reports "no output" instead of the actual Python error.

**4.5 `new_level()` truncates the RC response.** The world switch tears down the
call context; anything after it in the same script runs but returns nothing.
Level creation must be its own call.

**4.6 Exposure must be pinned, but not with `AEM_MANUAL`.**
`AutoExposureMethod.AEM_MANUAL` without camera aperture/shutter/ISO renders pure
black. Auto-exposure left on drifts frame to frame, which makes luminance deltas
meaningless. The working configuration is histogram metering clamped to a single
value, with instant adaptation so nothing drifts between frames:

| Setting | Value |
|---|---|
| `auto_exposure_method` | `AEM_HISTOGRAM` |
| `auto_exposure_min_brightness` | `0.06` |
| `auto_exposure_max_brightness` | `0.06` (equal to min — this is the pin) |
| `auto_exposure_bias` | `3.6` |
| `auto_exposure_speed_up` / `speed_down` | `100.0` (no adaptation ramp) |

My own attempt used min == max == `1.0` with no bias and rendered black; the
clamp value is a scene-average luminance target, so it must match the stage, and
the bias then does the lifting. **Also zero `motion_blur_amount`,
`film_grain_intensity`, and `vignette_intensity`** — motion blur in particular
smears a moving effect differently depending on sampling and destroys
frame-to-frame comparability.

**4.7 Rotator argument order.** `unreal.Rotator(pitch, yaw, roll)`. Passing
`(0, -90, 0)` to stand a plane up spins it about Z and leaves it lying flat. My
"back wall" was a second floor for most of this session.

**4.8 An enclosing backdrop box fights every camera.** Wings and a ceiling put
geometry between the preset cameras and the subject. An open cyclorama —
floor + back wall only — is correct.

**4.9 `bCaptureEveryFrame` defaults to true.** The engine logs
`"Scene capture with bCaptureEveryFrame enabled was told to update - major
inefficiency"`. A stage left in the level with an active capture component
re-renders the scene every editor frame. Set `capture_every_frame` and
`capture_on_movement` to `False` and drive it explicitly with `capture_scene()`.
This is a direct, measurable contributor to "the editor feels slow".

**4.10 A partially-built stage is worse than none.** After a crash the level
reloaded with its floor planes but no lights, and rendered black — which looks
exactly like an effect failure. Stage setup must be unconditional and
idempotent: wipe and rebuild, never "rebuild if missing".

## 4b. How the existing tooling performed

Recorded because "which of our tools earned their keep" is worth more than a
feature list, and because the failures below are fixable.

**Earned its keep:**

- **`RECaptureWorkflowTools.capture_viewport_to_disk`** was the ground truth that
  broke the whole investigation open. While my own capture rig returned black
  frames, this returned a correct image of the same scene — which proved the
  world rendered and the bug was mine. A second, independently-implemented path
  to the same observation is worth a great deal during debugging.
- **`RECaptureWorkflowTools.visual_loop_tool_notes`** is the right *idea*: a tool
  whose entire job is telling an agent what already exists (`already_exists_use_these`,
  `do_not_rebuild`, `epic_capture_pain`). It correctly steered me off
  `EditorAppToolset.CaptureViewport` (returns inline base64) and off rebuilding
  LogsToolset / SlateInspector. **Extend this pattern**; it is cheap and it
  directly attacks the "agent rebuilds what exists" failure.
- **Epic's schema-echo errors.** Calling `GetSystemSummary` with a wrong argument
  returned the full expected input schema alongside what was actually sent. That
  is a one-round-trip fix instead of a guessing loop, and our own tools should
  copy it verbatim.

**Did not help, and why:**

- **Reading the source beat reading the tools.** The working render-target recipe
  came from opening `capture_workflow_tools.py::_scenecapture_to_disk`, not from
  any tool output. Every landmine in §4 was found by trial, not documentation.
  This is the gap worth closing: the knowledge existed in the repo and was not
  reachable through the tool surface.
- **Tool names in prose did not match the registry.** `inspect_system` does not
  exist on `UeremcpNiagara.UeremcpNiagaraToolset`; it is referenced as though it
  does. An agent that trusts the prose burns a failed call.
- **Object-path shape cost two failed calls.** `GetSystemSummary` needs
  `{"system": {"refPath": "/Game/X/NS_Y.NS_Y"}}` — nested, *and* the full
  `Package.Asset` form. The asset path alone is rejected. Worth a `defs.schema`
  note and an example in every doc that mentions a Niagara tool.
- **`describe_toolset` is expensive discovery.** ~29 KB for one Niagara toolset.
  Discovering three toolsets costs more context than the work. A
  names-and-one-line-summary mode would pay for itself immediately.
- **No time-stepping tool exists anywhere.** `NiagaraToolset_Component` exposes
  four tools (`SetVariable`, `SetSystem`, `GetVariable`, `GetUserVariables`) and
  none of them touch age, seek, or simulation advance. The single most important
  primitive for deterministic visual verification is reachable only through raw
  Python. **This is the real gap** — not a broken tool, a missing domain.
- **`GetSystemSummary` is suspected of crashing the editor** (§6.3).

None of this is a fundamental flaw in the architecture. The pattern is consistent:
where a composite tool existed it worked and saved time; the failures are missing
coverage and stale naming, both of which are additive fixes.

## 5. Reference recipe

Validated end-to-end except for the particle question in §6.

> **Phasing is the whole trick.** Anything that requires the simulation or the
> renderer to advance — spawning an actor, seeking a Niagara age, advancing a
> montage — must happen in a *different* RC call from the capture that observes
> it. One call = one editor tick boundary. Collapsing them is the single most
> productive way to get a confidently wrong answer.

```python
# Phase 1 — rig (own call)
rt = unreal.RenderingLibrary.create_render_target2d(
        world, 640, 360, unreal.TextureRenderTargetFormat.RTF_RGBA8)  # 4 args only
cap = eas.spawn_actor_from_class(unreal.SceneCapture2D.static_class(), loc)
sc  = cap.get_component_by_class(unreal.SceneCaptureComponent2D)
sc.set_editor_property("texture_target", rt)
sc.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
sc.set_editor_property("capture_every_frame", False)   # see 4.9
sc.set_editor_property("capture_on_movement", False)

# Phase 2 — subject (own call; renderable from phase 3 onward, see 4.2)
nc.set_asset(system)
nc.set_age_update_mode(unreal.NiagaraAgeUpdateMode.DESIRED_AGE)
nc.set_seek_delta(1.0 / 30.0)
nc.set_can_render_while_seeking(True)
nc.activate(True)

# Phase 3..N — TWO calls per frame: step, THEN capture (see 6.1)
nc.seek_to_desired_age(age)          # call N   -- sets target; advance is on tick
# ---- call boundary: the editor ticks here ----
sc.capture_scene()                   # call N+1
stats = luminance_stats(rt)          # gate on this
unreal.RenderingLibrary.export_render_target(world, rt, outdir, "age_%02d.png" % i)
```

**Signal, per frame:** mean luminance, count of pixels above a luminance
threshold, max luminance — each compared against an empty-stage baseline captured
in the same session. Assert on the delta, never on the absolute.

> Note: actor **bounds extent** was evaluated as a particle-liveness signal and
> **rejected**. `NS_Explosion` reports a constant `[750, 750, 439]` at every age
> because its bounds are fixed, not dynamic. It reads like a simulation signal
> and is not one.

## 6. Open questions — do not automate past these

**6.1 RESOLVED — the missing call was `SetForceSolo(True)`.** My harness never
made a Niagara system appear, and I wrongly floated "editor world may be
insufficient, PIE may be required" as the leading read. It is not. The operator's
existing `Scripts/capture_ice_wall_baseline0.py` has been capturing deterministic
Niagara ages in an editor world all along. The sequence is:

```python
component.set_force_solo(True)      # <- the unlock
...
component.deactivate()
component.reinitialize_system()
component.activate(True)
component.advance_simulation(round(age / delta), delta)
```

`SetForceSolo` takes the component out of the batched simulation and gives it its
own solo instance, which is what allows `AdvanceSimulation` to step it
**synchronously**. Without it, `AdvanceSimulation` does not drive the component
and every frame captures the un-simulated state. `SeekToDesiredAge` — what I
reached for — is the wrong primitive here; it defers to tick.

**RB-07 q17 is therefore answered in the cheap direction: PIE is not required.**
An editor world, a solo component, and `AdvanceSimulation` are sufficient. That
is the good branch, and it means visual verification is affordable.

Note the recipe re-simulates from zero for *every* frame rather than stepping
incrementally. That costs more simulation but makes frame *N* a pure function of
its age — independent of capture order and of anything that happened before it.
Prefer this; incremental stepping trades away the property that makes the
harness a regression gate.

**6.2 RESOLVED — see 4.6** for the exposure configuration that holds still.

**6.3 `NiagaraToolset_System.GetSystemSummary` is suspected of crashing the
editor.** With a valid `system.refPath`, the editor terminated with the log
ending on that dispatch. A separate earlier dump in the same project shows
`EXCEPTION_ACCESS_VIOLATION` with `UnrealEditor_NiagaraEditor` as the top frame.
**One occurrence, correlation not confirmed repro** — it was not deliberately
re-run, because reproducing it costs a live editor. Treat as suspect; isolate
before calling it from any automated path.

**6.4 Capture cost.** One `capture_viewport_to_disk` through the existing
REAgentTools composite measured **33.9 s**. Whether that is capture cost, editor
contention, or the `bCaptureEveryFrame` inefficiency of 4.9 is not yet separated.
This number, not the token count, is what makes visual verification feel
unaffordable, and it should be measured properly before the harness is sold on
performance grounds.

## 7. Does this actually improve performance?

Split the claim, because the honest answers differ:

- **Round trips per verification: yes, substantially.** Framing, aiming and
  "when do I screenshot" currently cost several exploratory calls each, and they
  are re-derived per effect. A preset stage collapses them to zero. This is the
  `r·N²/2` term, so the saving compounds.
- **Determinism: yes, and this is the larger win.** Frame *i* meaning a fixed
  simulation age is what makes a *regression* test possible at all. Without it
  there is no comparison across runs, only impressions.
- **Engine speed: no.** None of this makes Niagara compile faster or the editor
  boot faster. If the felt slowness is compile waits or editor restarts, a better
  rig does not touch it. Fixing 4.9 is a real but bounded win.
- **Context budget: this is the one place payload size genuinely matters.** Per
  [`WHY.md`](WHY.md) payload is normally near-free, but images consume context
  *capacity*, not just tokens. Return **disk paths and numbers by default**,
  exactly as `RECaptureWorkflowTools` already does (*"never return base64"*), and
  emit one tiled contact sheet rather than N separate images when a human needs
  to look.

## 8. Animation

Lower priority than Niagara, deliberately. `inspect_montage` / `read_anim_bp`
already return notify timing, sections, and bone data as structured state — which
answers "is it playing correctly" without pixels at all. Pixels only answer "does
it *look* right", which is closer to art direction than to agent-verifiable
correctness. The same stage and the same phased capture apply when needed; drive
the pose with Sequencer scrub (`SequencerTools`, already registered) instead of
`seek_to_desired_age`.

## 9. The MCP tool

Shipped as `capture_effect_frames` on `UeremcpValidation.UeremcpVisualCaptureToolset`.
It lives in the **validation** module, not the Niagara module, because it answers
"does the change look right" rather than authoring anything — and per
`WORK_ALLOCATION.md` that module is WS-11's.

Automatic by design: the agent supplies an asset path, and the tool builds its
own stage, frames the subject, steps the simulation, writes frames, and tears
every spawned actor down again — including on failure.

**Request**

```json
{
  "action": "capture_effect_frames",
  "target": { "asset_path": "/Game/RE/VFX/Magecraft/Spells/NS_Spell_IceWall_Cast" },
  "specification": {
    "frame_count": 8,
    "duration_seconds": 1.5,
    "camera": "three_quarter",
    "width": 960,
    "height": 540
  }
}
```

Only `target.asset_path` is required. `camera` is one of `front`,
`three_quarter`, `side`, `top`.

**Response** — one entry per frame with its exact simulation age, the PNG path,
and luminance measurements against an empty-stage baseline, plus:

```json
"verification": {
  "rendered_something": true,
  "max_delta_lit_pixels": 30454,
  "baseline_lit_pixels": 91243,
  "note": "rendered_something is a pixel-delta gate, not an appearance judgement"
}
```

**Paths, never image bytes.** Consistent with `RECaptureWorkflowTools`' existing
rule (*"never return base64"*). Per §7, images consume context *capacity*, which
is the one place payload size genuinely matters.

**Round-trip cost:** one call for a whole frame series. The baseline arm — spawn,
aim, capture, look, re-aim, re-capture, repeat per age — is a double-digit call
count with no determinism at the end of it.

### Status

**The C++ implementation is written but NOT COMPILED OR RUN.** The editor was
unavailable at the time of writing. It transcribes a recipe proven in Python, so
the sequence is sound, but the C++ has never been through a compiler and must be
treated as unverified until it has. Per `AGENTS.md` rule 6, tool-call completion
is not success.

**It is also not in the checkout the editor loads.** `visualtest/Plugins/UEREMCP`
is a symlink to `GitHub/UEREMCP-ws01/Plugins/UEREMCP` (currently on
`ws-07-niagara-status-honesty`), while this work is on `ws-11-editor-gate-runtime-followup`
in the main clone. The tool cannot appear in `list_toolsets` until that branch
reaches the linked checkout — worth knowing before anyone concludes the
registration is broken.

**Conformance:** goes through `FUeremcpEnvelope::ParseRequest` /
`IsProtocolCompatible` / `MakeRejection` / `SerializeResponse` like every other
UEREMCP toolset, honours `dry_run`, and reports nothing in `created`/`modified`
because it authors nothing. It does **not** yet use WS-12's path policy or the
mutating-dispatch gate — it spawns actors into the current editor world and
writes PNGs under `Saved/`. Both are arguably out of scope for a read-only
observer, but that is WS-12's call, not WS-11's.

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Validation"
```

### Acceptance criteria before the capability is claimed

1. The module compiles and `UUeremcpVisualCaptureToolset` appears in
   `list_toolsets`.
2. A stock Niagara system produces `rendered_something: true` with a
   pixel-delta gate proving it.
3. The same age produces the same measurement twice in one session, and across
   an editor restart.
4. An empty-stage series has flat luminance (exposure genuinely pinned, per 4.6).
5. Teardown verified by asserting the actor count returns to its starting value
   after both a success and a forced failure.
6. Per-frame capture cost measured and reported, not assumed (per 6.4).

Until 1–2 hold, `capability_catalog` must not mark this `available`. Shipping a
tool that silently returns black frames would manufacture confidence in exactly
the place the project cannot afford it — see `tests/README.md`, *Writing an
honest test*.

## 10. Frame assembly

Frames on disk are the raw material; agents and humans want one artefact. The
operator's `Scripts/assemble_ice_wall_baseline0.py` builds a **GIF plus a contact
sheet** with Pillow, which is the right split:

- **contact sheet** for an agent — every age visible in one image, one attachment
  instead of N;
- **GIF** for a human — motion reads instantly and is far easier to judge.

Assembly stays outside the MCP tool: Pillow is not available in-engine, the tool
already returns a directory, and keeping composition separate means it can be
re-run without re-simulating.
