"""Deterministic VFX capture stage — reference implementation.

Protocol and the list of landmines this encodes:
    docs/VISUAL_CAPTURE_PROTOCOL.md

Status: the rig (stage, camera, capture, per-frame numeric signal) is validated
live. Particle capture is NOT yet achieved -- see protocol section 6.1. Do not
wire this into a gate until that is resolved.

Drives the editor's Python over Remote Control (:30010). Requires the editor
running with `WebControl.StartServer` (visualtest/Content/Python/init_unreal.py
already does this).

Usage:
    python tests/visual/vfx_capture_stage.py build
    python tests/visual/vfx_capture_stage.py shoot /Game/Path/NS_Thing MyTag
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile

RC_URL = "http://127.0.0.1:30010/remote/object/call"
PY_LIB = "/Script/PythonScriptPlugin.Default__PythonScriptLibrary"

STAGE_LEVEL = "/Game/__UeremcpTests/VisualStage/L_VfxCaptureStage"
STAGE_DIR = "/Game/__UeremcpTests/VisualStage"
BACKDROP_MAT = STAGE_DIR + "/M_StageBackdrop"

# Subject sits at the origin; cameras are authored once, relative to it.
CAMERA_PRESETS = {
    "front":         (-900,     0, 300),
    "three_quarter": (-820,  -560, 360),
    "side":          (   0,  -950, 300),
    "top":           (-300,     0, 1050),
}
AIM = (0, 0, 210)


# --------------------------------------------------------------------------
# Remote Control transport
# --------------------------------------------------------------------------
def rc_python(code: str, timeout: int = 600) -> list[tuple[str, str]]:
    """Run Python in the editor. Returns [(severity, text), ...].

    Tracebacks arrive in CommandResult, NOT LogOutput (protocol 4.4) -- a client
    that reads only LogOutput reports "no output" for every failure.
    """
    payload = {
        "objectPath": PY_LIB,
        "functionName": "ExecutePythonCommandEx",
        "parameters": {
            "PythonCommand": code,
            "ExecutionMode": "ExecuteFile",
            "FileExecutionScope": "Private",
        },
        "generateTransaction": False,
    }
    handle = tempfile.NamedTemporaryFile(
        suffix=".json", delete=False, mode="w", encoding="utf-8")
    json.dump(payload, handle)
    handle.close()
    try:
        proc = subprocess.run(
            ["curl", "-s", "-m", str(timeout), "-X", "PUT", RC_URL,
             "-H", "Content-Type: application/json",
             "--data-binary", "@" + handle.name],
            capture_output=True)
    finally:
        os.unlink(handle.name)

    try:
        data = json.loads(proc.stdout.decode("utf-8", "replace"))
    except ValueError:
        return [("Error", "no/invalid RC response -- editor down?")]

    out: list[tuple[str, str]] = []
    result = data.get("CommandResult")
    if result and result not in ("None", "True", "False"):
        kind = "Error" if "Traceback" in str(result) else "Info"
        out.append((kind, str(result).rstrip()))
    if data.get("ReturnValue") is False:
        out.append(("Error", "script raised"))
    for entry in data.get("LogOutput", []):
        out.append((entry.get("Type"), entry.get("Output", "").rstrip()))
    return out


def rc_json(code: str, timeout: int = 600):
    """Run code that prints one `@@<json>` line; return the parsed object."""
    parsed = None
    for kind, text in rc_python(code, timeout):
        if kind == "Error" or "Traceback" in text:
            print("!! " + text[:3000])
            continue
        for line in text.splitlines():
            if line.startswith("@@"):
                parsed = json.loads(line[2:])
    return parsed


PRELUDE = """
import unreal, json
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

def find(label):
    for a in (eas.get_all_level_actors() or []):
        if a.get_actor_label() == label:
            return a
    return None

def spawn(cls, loc, rot=(0, 0, 0), label=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    if a and label:
        a.set_actor_label(label, True)
    return a

def shoot(sc, rt, tag, name):
    # One capture_scene() can export a stale/empty frame. Clear, then capture
    # repeatedly before exporting (protocol 4.2).
    unreal.RenderingLibrary.clear_render_target2d(
        world, rt, unreal.LinearColor(0.005, 0.008, 0.015, 1.0))
    for _ in range(3):
        sc.capture_scene()
    unreal.RenderingLibrary.export_render_target(
        world, rt, unreal.Paths.project_saved_dir() + "UEREMCP/VfxCapture/" + tag,
        name + ".png")

def luminance_stats(rt):
    cols = unreal.RenderingLibrary.read_render_target(world, rt)
    n = len(cols)
    total = 0.0
    lit = 0
    peak = 0.0
    for c in cols:
        l = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
        total += l
        if l > 40:
            lit += 1
        if l > peak:
            peak = l
    return {"mean": round(total / max(n, 1), 2), "lit": lit, "max": round(peak, 1)}
"""


# --------------------------------------------------------------------------
# Phase 1 -- stage. Unconditional rebuild: a half-built stage renders black and
# is indistinguishable from a broken effect (protocol 4.10).
# --------------------------------------------------------------------------
BUILD_STAGE = PRELUDE + """
res = {"errors": []}
for a in list(eas.get_all_level_actors() or []):
    eas.destroy_actor(a)

plane = unreal.load_asset("/Engine/BasicShapes/Plane")
mat = unreal.load_asset("%(MAT)s")

# Open cyclorama only. Wings/ceiling put geometry between every preset camera
# and the subject (protocol 4.8).
# unreal.Rotator is (pitch, yaw, roll): pitch -90 stands a plane up facing -X.
for label, loc, rot, scale in [
        ("Stage_Floor",    (0, 0, 0),   (0, 0, 0),   (30, 30, 1)),
        ("Stage_BackWall", (900, 0, 0), (-90, 0, 0), (30, 30, 1))]:
    a = spawn(unreal.StaticMeshActor, loc, rot, label)
    smc = a.get_component_by_class(unreal.StaticMeshComponent)
    smc.set_static_mesh(plane)
    if mat:
        smc.set_material(0, mat)
    a.set_actor_scale3d(unreal.Vector(*scale))

key = spawn(unreal.DirectionalLight, (0, 0, 1000), (-42, -140, 0), "Key_Directional")
c = key.get_component_by_class(unreal.DirectionalLightComponent)
c.set_intensity(2.0)
c.set_light_color(unreal.LinearColor(0.85, 0.90, 1.0, 1.0))

sky = spawn(unreal.SkyLight, (0, 0, 700), (0, 0, 0), "Ambient_SkyLight")
c = sky.get_component_by_class(unreal.SkyLightComponent)
c.set_intensity(0.25)
c.set_light_color(unreal.LinearColor(0.55, 0.62, 0.80, 1.0))

fill = spawn(unreal.PointLight, (-450, 420, 320), (0, 0, 0), "Fill_Point")
c = fill.get_component_by_class(unreal.PointLightComponent)
c.set_intensity(18000.0)
c.set_attenuation_radius(1800.0)
c.set_light_color(unreal.LinearColor(1.0, 0.93, 0.82, 1.0))

rim = spawn(unreal.PointLight, (480, -340, 460), (0, 0, 0), "Rim_Point")
c = rim.get_component_by_class(unreal.PointLightComponent)
c.set_intensity(24000.0)
c.set_attenuation_radius(2000.0)
c.set_light_color(unreal.LinearColor(0.62, 0.78, 1.0, 1.0))

res["actors"] = [a.get_actor_label() for a in (eas.get_all_level_actors() or [])]
print("@@" + json.dumps(res, default=str))
""" % {"MAT": BACKDROP_MAT}


# --------------------------------------------------------------------------
# Phase 2 -- capture rig. Separate call: actors spawned in call N are not
# renderable until call N+1 (protocol 4.2).
# --------------------------------------------------------------------------
BUILD_RIG = PRELUDE + """
res = {}
a = find("Stage_Capture")
if a:
    eas.destroy_actor(a)

# 4-arg form ONLY. The 6-arg overload yields a target that captures pure black
# forever, silently (protocol 4.1). RTF_RGBA8 or export writes EXR bytes (4.3).
rt = unreal.RenderingLibrary.create_render_target2d(
    world, %(W)d, %(H)d, unreal.TextureRenderTargetFormat.RTF_RGBA8)

loc = unreal.Vector(%(CX)f, %(CY)f, %(CZ)f)
rot = unreal.MathLibrary.find_look_at_rotation(loc, unreal.Vector(%(AX)f, %(AY)f, %(AZ)f))
cap = eas.spawn_actor_from_class(unreal.SceneCapture2D.static_class(), loc)
cap.set_actor_label("Stage_Capture", True)
cap.set_actor_rotation(rot, False)

sc = cap.get_component_by_class(unreal.SceneCaptureComponent2D)
sc.set_editor_property("texture_target", rt)
sc.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
# Defaults to True and re-renders the scene every editor frame -- the engine
# itself logs this as a "major inefficiency" (protocol 4.9).
sc.set_editor_property("capture_every_frame", False)
sc.set_editor_property("capture_on_movement", False)
sc.set_editor_property("fov_angle", 52.0)

# Exposure must hold still or luminance deltas are meaningless. AEM_MANUAL
# renders black without camera settings; clamp histogram min == max instead,
# with instant adaptation so nothing ramps between frames (protocol 4.6).
# Motion blur especially must go -- it smears a moving effect differently
# depending on sampling.
post = unreal.PostProcessSettings()
for prop, value in (
        ("override_auto_exposure_method", True),
        ("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM),
        ("override_auto_exposure_min_brightness", True),
        ("auto_exposure_min_brightness", 0.06),
        ("override_auto_exposure_max_brightness", True),
        ("auto_exposure_max_brightness", 0.06),
        ("override_auto_exposure_bias", True),
        ("auto_exposure_bias", 3.6),
        ("override_auto_exposure_speed_up", True),
        ("auto_exposure_speed_up", 100.0),
        ("override_auto_exposure_speed_down", True),
        ("auto_exposure_speed_down", 100.0),
        ("override_motion_blur_amount", True),
        ("motion_blur_amount", 0.0),
        ("override_film_grain_intensity", True),
        ("film_grain_intensity", 0.0),
        ("override_vignette_intensity", True),
        ("vignette_intensity", 0.0),
        ("override_bloom_intensity", True),
        ("bloom_intensity", 0.25)):
    try:
        post.set_editor_property(prop, value)
    except Exception as exc:
        # NB: this template is percent-substituted, so format markers are doubled
        res.setdefault("pp_warnings", []).append("%%s: %%s" %% (prop, exc))
sc.set_editor_property("post_process_settings", post)

res["camera"] = [loc.x, loc.y, loc.z]
print("@@" + json.dumps(res, default=str))
"""


# --------------------------------------------------------------------------
# Phase 3 -- empty-stage baseline, then spawn the subject.
# --------------------------------------------------------------------------
BASELINE_AND_SPAWN = PRELUDE + """
res = {}
cap = find("Stage_Capture")
sc = cap.get_component_by_class(unreal.SceneCaptureComponent2D)
rt = sc.get_editor_property("texture_target")
shoot(sc, rt, "%(TAG)s", "baseline")
res["baseline"] = luminance_stats(rt)

a = find("VFX_Subject")
if a:
    eas.destroy_actor(a)
asset = unreal.load_asset("%(SYSTEM)s")
if not asset:
    res["errors"] = ["load failed: %(SYSTEM)s"]
    print("@@" + json.dumps(res, default=str))
else:
    subj = spawn(unreal.NiagaraActor, (0, 0, 60), (0, 0, 0), "VFX_Subject")
    nc = subj.get_component_by_class(unreal.NiagaraComponent)
    nc.set_asset(asset, True)
    # THE critical call. Without solo the component rides the batched simulation
    # and advance_simulation will not step it synchronously in an editor world.
    nc.set_force_solo(True)
    res["subject"] = asset.get_name()
    print("@@" + json.dumps(res, default=str))
"""


# --------------------------------------------------------------------------
# Phase 4..N -- one call per frame.
#
# Re-simulates from zero every frame rather than stepping incrementally. That
# costs more simulation but makes frame N a pure function of its age --
# independent of capture order and of anything that happened before it. That
# property is what makes this a regression gate rather than an impression.
#
# seek_to_desired_age is NOT used: it defers the advance to the component's
# tick. advance_simulation on a force-solo component steps synchronously.
# --------------------------------------------------------------------------
SHOOT_FRAME = PRELUDE + """
res = {}
cap = find("Stage_Capture")
sc = cap.get_component_by_class(unreal.SceneCaptureComponent2D)
rt = sc.get_editor_property("texture_target")
nc = find("VFX_Subject").get_component_by_class(unreal.NiagaraComponent)

nc.deactivate()
nc.reinitialize_system()
nc.activate(True)
age = %(AGE)f
if age > 0.0:
    nc.advance_simulation(max(1, int(round(age / 0.0166667))), 0.0166667)

shoot(sc, rt, "%(TAG)s", "age_%(IDX)02d")
res["age"] = age
res["stats"] = luminance_stats(rt)
print("@@" + json.dumps(res, default=str))
"""


def build(camera: str = "three_quarter", width: int = 640, height: int = 360):
    print("stage:", rc_json(BUILD_STAGE))
    cx, cy, cz = CAMERA_PRESETS[camera]
    ax, ay, az = AIM
    print("rig:", rc_json(BUILD_RIG % {
        "W": width, "H": height,
        "CX": cx, "CY": cy, "CZ": cz, "AX": ax, "AY": ay, "AZ": az}))


def shoot(system: str, tag: str, ages=(0.05, 0.15, 0.3, 0.5, 0.75, 1.0, 1.3, 1.7)):
    head = rc_json(BASELINE_AND_SPAWN % {"SYSTEM": system, "TAG": tag})
    if not head:
        print("baseline/spawn failed"); return
    base = head.get("baseline", {})
    print("baseline:", base, "subject:", head.get("subject"))

    # A black baseline means the render target is invalid, not that the scene is
    # empty -- every downstream comparison would be meaningless (protocol 4.1).
    if base.get("max", 0) < 5:
        print("ABORT: baseline frame is black; render target is not valid")
        return

    for i, age in enumerate(ages):
        r = rc_json(SHOOT_FRAME % {"AGE": age, "IDX": i, "TAG": tag})
        if not r:
            print("  age %.2f FAILED" % age)
            continue
        s = r["stats"]
        print("  age=%-5.2f mean=%-7.2f lit=%-7d max=%-6.1f  d_lit=%+d"
              % (age, s["mean"], s["lit"], s["max"], s["lit"] - base.get("lit", 0)))


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "build"
    if cmd == "build":
        build(*(sys.argv[2:3] or ["three_quarter"]))
    elif cmd == "shoot":
        shoot(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "Untagged")
    else:
        print(__doc__)
