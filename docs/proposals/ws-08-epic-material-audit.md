# Proposal: Epic MaterialTools audit rows for WS-02

- **From:** WS-08
- **To:** WS-02 (`docs/audit/epic-toolsets.md`, capability matrix)
- **Date:** 2026-07-29

## Request

Merge the following disposition rows into the Epic audit matrix. All tool names verified from `$ENGINE/Plugins/Experimental/Toolsets/EditorToolset/Content/Python/editor_toolset/toolsets/material.py` and `material_instance.py`.

## Epic MaterialTools — disposition: **Preserve (internal only)**

| Tool | Purpose | UEREMCP disposition |
|---|---|---|
| `create_material` | Empty material asset | Internal primitive; hidden from agent |
| `create_function` | Empty material function | Internal primitive |
| `create_parameter_collection` | MPC asset | Internal; rare agent face |
| `list_expression_classes` | Discover expression types | Internal; semantic tools pick classes |
| `add_expression` | Add graph node | Internal |
| `delete_expression` | Remove graph node | Internal |
| `get_expressions` | List nodes | Internal / graph read adapter |
| `layout_expressions` | Auto-layout | Internal optional |
| `list_parameter_groups` | Parameter UI groups | Graph read |
| `rename_parameter_group` | Group rename | Internal |
| `delete_parameter_group` | Ungroup parameters | Internal |
| `get_expression_input_names` | Pin discovery | Graph read |
| `get_expression_output_names` | Pin discovery | Graph read |
| `connect_expressions` | Wire nodes | Internal |
| `disconnect_expressions` | Unwire pin | Internal |
| `get_expression_inputs` | Read wiring | Graph read |
| `get_property_input` | Read output property source | Graph read |
| `connect_to_output` | Wire to MP_* | Internal |
| `disconnect_from_output` | Unwire MP_* | Internal |
| `delete_unused_expressions` | Cleanup | Internal |
| `recompile` | Shader compile + errors | Validation layer |
| `get_referencing_materials` | Function referencers | Diagnostics |

## Epic MaterialInstanceTools — disposition: **Preserve / Improve via envelope**

| Tool | Purpose | UEREMCP disposition |
|---|---|---|
| `create` | Create MIC | Improve — envelope + idempotency |
| `list_parameters` | Parameter manifest | Return in semantic response |
| `get/set_scalar_parameter` | Scalar MI override | Improve — batch in `create_vfx_material` |
| `get/set_vector_parameter` | Vector MI override | Improve — batch |
| `get/set_texture_parameter` | Texture MI override | Improve — batch |
| `get/set_static_switch_parameter` | Static switch | Improve — warn on recompile cost |
| `set_parent` | Reparent MI | Internal |
| `clear_parameters` | Reset overrides | Internal; dry_run default |
| `set_parameter_override` | Toggle override flag | Internal |

## REAgentTools REMaterialWorkflowTools — disposition

| Tool | Disposition | Notes |
|---|---|---|
| `create_material_instance_configure_save` | **Improve** | Wrap in UEREMCP envelope; keep JSON params |
| `update_material_instance_parameters` | **Improve** | Same |
| `assign_materials_to_mesh_components` | **Preserve** | Not WS-08 core; actor workflow |
| `create_assign_material_instance` | **Improve** | Composite valid for quick assign |
| Master material graph edit | **Absent** | Epic MaterialTools fills gap |

## UEREMCP additions (not duplicates)

| Planned action | Why not duplicate Epic |
|---|---|
| `create_vfx_material` | One semantic op; batches MaterialTools + MI + validation |
| `retrieve_material_graph` | ADR-0004 JSON; Epic returns object refs |
| `replace_material_graph` | ADR-0004 round-trip |
| `create_procedural_texture` | No Epic equivalent |
| `instantiate_element_material` | Element template + parameter model |
