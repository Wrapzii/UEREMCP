import importlib.util
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[4] / "Unreal Projects" / "RE" / "Content" / "Python" / "_dump_unreal_mcp_tools.py"


class RegistryParserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not SCRIPT.exists():
            raise unittest.SkipTest("RE project is not present beside UEREMCP")
        spec = importlib.util.spec_from_file_location("re_dump_tools", SCRIPT)
        cls.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.module)

    def test_embedded_bullets_are_not_toolsets(self):
        text = "\n".join([
            "- UeremcpCore.UeremcpReferenceToolset: core",
            "  - GetAssetDiscoveryInfo: embedded Niagara bullet",
            "  - FindNiagaraScripts: embedded Niagara bullet",
            "  - GetNiagaraScriptDigest: embedded Niagara bullet",
            "- NiagaraToolsets.NiagaraToolset_System: system",
        ])
        self.assertEqual(
            self.module.parse_toolsets_list(text),
            ["UeremcpCore.UeremcpReferenceToolset", "NiagaraToolsets.NiagaraToolset_System"],
        )


if __name__ == "__main__":
    unittest.main()
