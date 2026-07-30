"""Offline contract checks for Material ADR-0010 dispatcher adoption."""

from pathlib import Path
from typing import Optional


MODULE_ROOT = Path(__file__).resolve().parents[1]
TOOLSET = MODULE_ROOT / "Private" / "UeremcpMaterialToolset.cpp"
TESTS = MODULE_ROOT / "Private" / "Tests" / "UeremcpMaterialToolsetTests.cpp"
BUILD = MODULE_ROOT / "UeremcpMaterial.Build.cs"


def function_body(source: str, function_name: str, next_function_name: Optional[str]) -> str:
    start = source.index(f"UUeremcpMaterialToolset::{function_name}")
    end = source.index(
        f"UUeremcpMaterialToolset::{next_function_name}", start
    ) if next_function_name else len(source)
    return source[start:end]


def main() -> None:
    toolset = TOOLSET.read_text(encoding="utf-8")
    build = BUILD.read_text(encoding="utf-8")
    tests = TESTS.read_text(encoding="utf-8")

    vfx = function_body(toolset, "CreateVfxMaterial", "CreateProceduralTexture")
    texture = function_body(toolset, "CreateProceduralTexture", None)
    for name, body in (
        ("CreateVfxMaterial", vfx),
        ("CreateProceduralTexture", texture),
    ):
        assert "MutatingDispatch.TryBegin(" in body, f"{name} must call TryBegin"
        assert "MutatingDispatch.Complete(Response)" in body, f"{name} must call Complete"
        assert "MutatingDispatch.IsEffectiveDryRun()" in body, (
            f"{name} must propagate policy-forced dry_run"
        )

    assert '"UeremcpSecurity"' in build
    assert "MutatingDispatchPathPolicy" in tests
    assert "DestructiveReplaceForcesDryRun" in tests
    assert "ConcurrentCreateQueues" in tests
    print("UeremcpMaterial security contract checks passed.")


if __name__ == "__main__":
    main()
