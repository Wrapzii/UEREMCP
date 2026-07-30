"""Executable pre-UEREMCP/Epic primitive baseline for the POC-B fireball.

Pass this entire file as ProgrammaticToolset.execute_tool_script's ``script``
argument. The client harness, not this script, owns wall-clock measurement.
"""

import json
import time


TRACE = []


def call(tool_name, arguments):
    """Execute and count one inner primitive operation."""
    started = time.monotonic()
    try:
        result = execute_tool(tool_name, json.dumps(arguments))
        TRACE.append(
            {
                "tool": tool_name,
                "ok": True,
                "elapsed_ms": (time.monotonic() - started) * 1000.0,
            }
        )
        return result
    except Exception as error:
        TRACE.append(
            {
                "tool": tool_name,
                "ok": False,
                "elapsed_ms": (time.monotonic() - started) * 1000.0,
                "error": str(error),
            }
        )
        raise


def value(tool_name, arguments):
    return call(tool_name, arguments)["returnValue"]


def object_ref(package_path):
    asset_name = package_path.rsplit("/", 1)[-1]
    return {"refPath": package_path + "." + asset_name}


def stack_ref(system, emitter_name, renderer_index=-1):
    return {
        "system": system,
        "emitterName": emitter_name,
        "scriptName": "",
        "moduleName": "",
        "rendererIndex": renderer_index,
        "inputNameStack": [],
    }


def binding_path(property_values, renderer_class):
    if "SpriteRenderer" in renderer_class or "RibbonRenderer" in renderer_class:
        material = property_values["Material"]
        return material["refPath"]
    if "MeshRenderer" in renderer_class:
        overrides = property_values["OverrideMaterials"]
        if not property_values["bOverrideMaterials"] or not overrides:
            return ""
        return overrides[0]["ExplicitMat"]["refPath"]
    return ""


def patch_renderer(property_values_json, renderer_class, material_ref):
    properties = json.loads(property_values_json)
    if "SpriteRenderer" in renderer_class or "RibbonRenderer" in renderer_class:
        binding = properties.get("MaterialUserParamBinding")
        if binding and binding.get("Parameter", {}).get("Name") not in (None, "", "None"):
            raise RuntimeError("renderer has a user material binding that overrides Material")
        properties["Material"] = material_ref
    elif "MeshRenderer" in renderer_class:
        overrides = properties.get("OverrideMaterials", [])
        if not overrides:
            raise RuntimeError("mesh renderer exposes no material override slots")
        properties["bOverrideMaterials"] = True
        for override in overrides:
            override["ExplicitMat"] = material_ref
    else:
        raise RuntimeError("unsupported renderer class: " + renderer_class)
    return json.dumps(properties, separators=(",", ":"))


def run():
    global TRACE
    TRACE = []
    started = time.monotonic()

    system_package = "/Game/__UeremcpPoc/NS_POCB_Fireball_Baseline"
    material_folder = "/Game/__UeremcpPoc/Materials"
    roles = [
        {
            "role": "core",
            "emitter": "Core",
            "template": "/Niagara/DefaultAssets/Templates/Emitters/Minimal",
            "parent": "/Game/RE/VFX/Magecraft/RuntimeMaterials/M_FX_FireboltCore",
        },
        {
            "role": "flame_shell",
            "emitter": "FlameShell",
            "template": "/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst",
            "parent": "/Game/RE/VFX/Magecraft/RuntimeMaterials/M_FX_FireboltShell",
        },
        {
            "role": "sparks",
            "emitter": "Sparks",
            "template": "/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst",
            "parent": "/Game/RE/VFX/Magecraft/RuntimeMaterials/M_FX_FireboltSpark",
        },
        {
            "role": "smoke",
            "emitter": "Smoke",
            "template": "/Niagara/DefaultAssets/Templates/Emitters/Fountain",
            "parent": "/Game/RE/VFX/Magecraft/RuntimeMaterials/M_Niagara_Smoke",
        },
        {
            "role": "ribbon_trail",
            "emitter": "RibbonTrail",
            "template": "/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon",
            "parent": "/Game/RE/VFX/Magecraft/RuntimeMaterials/M_Niagara_FireRibbon",
        },
        {
            "role": "impact_burst",
            "emitter": "ImpactBurst",
            "template": "/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst",
            "parent": "/Game/RE/VFX/Magecraft/RuntimeMaterials/M_Niagara_ImpactFlash",
        },
    ]

    for role in roles:
        role["material_package"] = (
            material_folder
            + "/MI_NS_POCB_Fireball_Baseline_"
            + role["role"]
        )

    output_packages = [system_package] + [
        role["material_package"] for role in roles
    ]
    existing = []
    for package_path in output_packages:
        if value(
            "editor_toolset.toolsets.asset.AssetTools.exists",
            {"path": package_path},
        ):
            existing.append(package_path)
    if existing:
        return {
            "status": "blocked_dirty_target",
            "completed": False,
            "existing_outputs": existing,
            "primitive_ops_executed": len(TRACE),
            "primitive_trace": TRACE,
            "inner_elapsed_ms": (time.monotonic() - started) * 1000.0,
        }

    created = []
    try:
        for role in roles:
            material = value(
                "editor_toolset.toolsets.material_instance.MaterialInstanceTools.create",
                {
                    "folder_path": material_folder,
                    "asset_name": role["material_package"].rsplit("/", 1)[-1],
                    "parent": object_ref(role["parent"]),
                },
            )
            role["material_ref"] = material
            created.append(role["material_package"])

        system = value(
            "NiagaraToolsets.NiagaraToolset_System.CreateNiagaraSystem",
            {
                "assetName": "NS_POCB_Fireball_Baseline",
                "assetPath": "/Game/__UeremcpPoc",
                "templateSystem": object_ref(
                    "/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight"
                ),
            },
        )
        created.append(system_package)

        for role in roles:
            role["topology"] = value(
                "NiagaraToolsets.NiagaraToolset_System.AddEmitter",
                {
                    "system": system,
                    "templateEmitter": object_ref(role["template"]),
                    "emitterName": role["emitter"],
                },
            )

        variables = [
            {
                "name": "User.Color",
                "type": {
                    "classStructOrEnum": {
                        "refPath": "/Script/CoreUObject.LinearColor"
                    }
                },
                "defaultValue": {
                    "struct": {"refPath": "/Script/CoreUObject.LinearColor"},
                    "value": {"r": 1.0, "g": 0.12, "b": 0.01, "a": 1.0},
                },
                "description": "",
            },
            {
                "name": "User.SecondaryColor",
                "type": {
                    "classStructOrEnum": {
                        "refPath": "/Script/CoreUObject.LinearColor"
                    }
                },
                "defaultValue": {
                    "struct": {"refPath": "/Script/CoreUObject.LinearColor"},
                    "value": {"r": 1.0, "g": 0.75, "b": 0.05, "a": 1.0},
                },
                "description": "",
            },
            {
                "name": "User.Scale",
                "type": {
                    "classStructOrEnum": {
                        "refPath": "/Script/Niagara.NiagaraFloat"
                    }
                },
                "defaultValue": {
                    "struct": {"refPath": "/Script/Niagara.NiagaraFloat"},
                    "value": {"value": 1.0},
                },
                "description": "",
            },
            {
                "name": "User.Intensity",
                "type": {
                    "classStructOrEnum": {
                        "refPath": "/Script/Niagara.NiagaraFloat"
                    }
                },
                "defaultValue": {
                    "struct": {"refPath": "/Script/Niagara.NiagaraFloat"},
                    "value": {"value": 8.0},
                },
                "description": "",
            },
        ]
        call(
            "NiagaraToolsets.NiagaraToolset_System.AddUserVariables",
            {"system": system, "variablesToAdd": variables},
        )

        for role in roles:
            renderers = role["topology"]["renderers"]
            if not renderers:
                raise RuntimeError(role["emitter"] + " has no renderer")
            for renderer in renderers:
                index = renderer["rendererIndex"]
                renderer_class = renderer["rendererClass"]["refPath"]
                renderer_ref = stack_ref(system, role["emitter"], index)
                renderer_data = value(
                    "NiagaraToolsets.NiagaraToolset_System.GetRendererData",
                    {"rendererRef": renderer_ref},
                )
                patched = patch_renderer(
                    renderer_data["propertyValues"],
                    renderer_class,
                    role["material_ref"],
                )
                call(
                    "NiagaraToolsets.NiagaraToolset_System.SetRendererData",
                    {
                        "renderer": renderer_ref,
                        "rendererData": {"propertyValues": patched},
                    },
                )

        compile_state = value(
            "NiagaraToolsets.NiagaraToolset_System.GetSystemCompileState",
            {"system": system},
        )
        accepted_compile_states = [
            "UpToDate",
            "UpToDateWithWarnings",
            "ComputeUpToDateWithWarnings",
        ]
        if (
            compile_state["bIsCompiling"]
            or compile_state["bIsStale"]
            or compile_state["bHasErrors"]
            or compile_state["aggregateStatus"] not in accepted_compile_states
        ):
            raise RuntimeError(
                "compile did not settle successfully: "
                + json.dumps(compile_state, default=str)
            )

        saved = value(
            "editor_toolset.toolsets.asset.AssetTools.save_assets",
            {"asset_paths": output_packages},
        )
        if not saved:
            raise RuntimeError("save_assets returned false")

        summary = value(
            "NiagaraToolsets.NiagaraToolset_System.GetSystemSummary",
            {"system": system},
        )
        actual_emitters = [
            emitter["emitterName"] for emitter in summary["emitters"]
        ]
        expected_emitters = [role["emitter"] for role in roles]
        if actual_emitters != expected_emitters:
            raise RuntimeError(
                "emitter mismatch: " + json.dumps(actual_emitters)
            )
        actual_variables = sorted(
            variable["name"] for variable in summary["userVariables"]
        )
        expected_variables = sorted(variable["name"] for variable in variables)
        if actual_variables != expected_variables:
            raise RuntimeError(
                "user-variable mismatch: " + json.dumps(actual_variables)
            )

        verified_bindings = []
        for role in roles:
            topology = value(
                "NiagaraToolsets.NiagaraToolset_System.GetEmitterTopology",
                {"emitterRef": stack_ref(system, role["emitter"])},
            )
            if not topology["renderers"]:
                raise RuntimeError(role["emitter"] + " lost all renderers")
            expected_material = role["material_ref"]["refPath"]
            for renderer in topology["renderers"]:
                index = renderer["rendererIndex"]
                renderer_class = renderer["rendererClass"]["refPath"]
                renderer_data = value(
                    "NiagaraToolsets.NiagaraToolset_System.GetRendererData",
                    {
                        "rendererRef": stack_ref(
                            system, role["emitter"], index
                        )
                    },
                )
                actual_material = binding_path(
                    json.loads(renderer_data["propertyValues"]),
                    renderer_class,
                )
                if actual_material != expected_material:
                    raise RuntimeError(
                        role["emitter"]
                        + " renderer "
                        + str(index)
                        + " material mismatch: "
                        + actual_material
                    )
                verified_bindings.append(
                    role["emitter"] + "/renderer_" + str(index)
                )

        for role in roles:
            asset_class = value(
                "editor_toolset.toolsets.asset.AssetTools.get_asset_class",
                {"asset_path": role["material_package"]},
            )
            if asset_class != "MaterialInstanceConstant":
                raise RuntimeError(
                    role["material_package"] + " is " + asset_class
                )

        dirty_outputs = []
        for package_path in output_packages:
            if value(
                "editor_toolset.toolsets.asset.AssetTools.is_dirty",
                {"asset_path": package_path},
            ):
                dirty_outputs.append(package_path)
        if dirty_outputs:
            raise RuntimeError(
                "outputs remained dirty after save: "
                + json.dumps(dirty_outputs)
            )

        return {
            "status": "created_and_validated",
            "completed": True,
            "system": system["refPath"],
            "created_outputs": output_packages,
            "emitters": actual_emitters,
            "user_variables": actual_variables,
            "renderer_bindings_verified": verified_bindings,
            "compile_state": compile_state,
            "primitive_ops_executed": len(TRACE),
            "primitive_trace": TRACE,
            "inner_elapsed_ms": (time.monotonic() - started) * 1000.0,
        }
    except Exception as error:
        return {
            "status": "failed_validation",
            "completed": False,
            "error": str(error),
            "created_before_failure": created,
            "primitive_ops_executed": len(TRACE),
            "primitive_trace": TRACE,
            "inner_elapsed_ms": (time.monotonic() - started) * 1000.0,
        }
