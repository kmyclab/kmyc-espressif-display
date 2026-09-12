"""Hardware-independent checks of the catalog/build contract."""

import copy
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("kmyc", ROOT / "tools/kmyc.py")
kmyc = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(kmyc)
PRESET = "wireless-p4-d101-panel-test"


class WorkspaceTests(unittest.TestCase):
    def setUp(self):
        self.data = kmyc.catalog(ROOT)
        self.preset = copy.deepcopy(self.data["preset"][PRESET])

    def test_current_preset_resolves_to_wireless_and_full_product(self):
        selected = kmyc.resolve(self.data, self.preset)
        self.assertEqual(selected["board"]["manufacturer"], "wireless")
        self.assertEqual(selected["board"]["manufacturer_name"], "启明云端")
        self.assertEqual(selected["board"]["target"], "esp32p4")
        self.assertEqual(selected["display"]["model"], "KMYC-D101-DSI4L-800X1280-A1")
        self.assertIsNone(selected["preset"]["assembly"])
        self.assertTrue(kmyc.source_paths(ROOT, selected, "sources"))

    def test_unimplemented_app_is_not_silently_substituted(self):
        self.preset["app"] = "lvgl-demo"
        with self.assertRaisesRegex(ValueError, "not implemented app"):
            kmyc.resolve(self.data, self.preset)

    def test_cross_board_adapter_is_rejected(self):
        self.data["adapter"][self.preset["adapter"]]["board"] = "different-board"
        with self.assertRaisesRegex(ValueError, "does not match"):
            kmyc.resolve(self.data, self.preset)

    def test_missing_app_capability_is_rejected(self):
        self.data["app"]["panel-test"]["requires"].append("touch")
        with self.assertRaisesRegex(ValueError, "lacks capabilities"):
            kmyc.resolve(self.data, self.preset)

    def test_unknown_panel_mode_is_rejected(self):
        self.data["adapter"][self.preset["adapter"]]["mode"] = "dsi4-unimplemented"
        with self.assertRaisesRegex(ValueError, "unsupported display mode"):
            kmyc.resolve(self.data, self.preset)

    def test_registered_assembly_does_not_imply_touch_works(self):
        self.preset["assembly"] = next(iter(self.data["assembly"]))
        with self.assertRaisesRegex(ValueError, "metadata-only"):
            kmyc.resolve(self.data, self.preset)

    def test_other_sdk_is_not_implicitly_accepted(self):
        self.preset["idf"] = "6.1.0"
        with self.assertRaisesRegex(ValueError, "SDK version"):
            kmyc.resolve(self.data, self.preset)

    def test_recipe_sources_cannot_escape_workspace(self):
        selected = kmyc.resolve(self.data, self.preset)
        selected["board"]["sources"] = [str(ROOT.parent / "outside.c")]
        with self.assertRaisesRegex(ValueError, "escapes workspace"):
            kmyc.source_paths(ROOT, selected, "sources")

    def test_planned_platforms_are_not_build_targets(self):
        self.assertEqual(set(self.data["platform"]), {"esp32p4"})

    def test_configuration_isolated_and_existing_settings_preserved(self):
        with tempfile.TemporaryDirectory(prefix="kmyc-test-") as directory:
            root = Path(directory) / "workspace"
            shutil.copytree(ROOT, root, ignore=shutil.ignore_patterns(
                "out", "build", "managed_components", "__pycache__"))
            data = kmyc.catalog(root)
            selected = kmyc.resolve(data, data["preset"][PRESET])
            output = kmyc.configure(root, selected, root / "out" / PRESET)
            config = output / "sdkconfig"
            config.write_text("CONFIG_KMYC_PATTERN_PERIOD_MS=3000\n", encoding="utf-8")
            kmyc.configure(root, selected, output)
            self.assertEqual(config.read_text(), "CONFIG_KMYC_PATTERN_PERIOD_MS=3000\n")
            defaults = root / "apps/panel-test/sdkconfig.defaults"
            defaults.write_text(defaults.read_text() + "CONFIG_NEW_SETTING=y\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Recipe changed"):
                kmyc.configure(root, selected, output)
            with self.assertRaisesRegex(ValueError, "escapes workspace"):
                kmyc.configure(root, selected, root / "apps/panel-test")

    def test_sdkconfig_cannot_select_incompatible_silicon(self):
        selected = kmyc.resolve(self.data, self.preset)
        with tempfile.TemporaryDirectory(prefix="kmyc-config-") as directory:
            path = Path(directory) / "sdkconfig"
            values = dict(selected["board"]["required_config"])
            values["CONFIG_IDF_TARGET"] = '"esp32p4"'
            path.write_text("\n".join(f"{key}={value}" for key, value in values.items()), encoding="utf-8")
            kmyc.verify_sdkconfig(selected, path)
            values["CONFIG_ESP32P4_SELECTS_REV_LESS_V3"] = "n"
            path.write_text("\n".join(f"{key}={value}" for key, value in values.items()), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Board requires"):
                kmyc.verify_sdkconfig(selected, path)


if __name__ == "__main__":
    unittest.main()
