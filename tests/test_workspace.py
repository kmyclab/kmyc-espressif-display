"""Hardware-independent checks of the catalog/build contract."""

import copy
import contextlib
import importlib.util
import io
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("kmyc", ROOT / "tools/kmyc.py")
kmyc = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(kmyc)
PRESET = "wireless-p4-d101-panel-test"
WAVESHARE_PRESET = "waveshare-pico-r3-d101-bist"
WAVESHARE_PANEL_PRESET = "waveshare-pico-r1-d101-panel"
WAVESHARE_TOUCH_PRESET = "waveshare-pico-r1-d101-touch"
D070_TOUCH_PRESET = "waveshare-pico-r1-d070-touch"
WIRELESS_D070_TOUCH_PRESET = "wireless-p4-d070-touch"


class WorkspaceTests(unittest.TestCase):
    def test_bridge_demo_uses_its_own_adapter_and_controller_capability(self):
        data = kmyc.catalog(ROOT)
        preset = data["preset"]["wireless-p4-d070-bridge-v12-demo"]
        selected = kmyc.resolve(data, preset)
        self.assertEqual(selected["app"]["id"], "interactive-demo")
        self.assertEqual(selected["adapter"]["id"], "wireless-tiny-d070-bridge-v12-dsi2")
        self.assertIn("controller-lifecycle", selected["adapter"]["capabilities"])
        self.assertEqual(selected["touch"]["controller"], "GT911")
        old = copy.deepcopy(preset)
        old["adapter"] = "wireless-tiny-d070-dsi2"
        with self.assertRaisesRegex(ValueError, "capabilities"):
            kmyc.resolve(data, old)

    def test_bridge_does_not_change_legacy_adapter_recipes(self):
        data = kmyc.catalog(ROOT)
        for name in ("wireless-p4-d070-touch", "waveshare-pico-r1-d101-touch"):
            selected = kmyc.resolve(data, data["preset"][name])
            self.assertNotIn("controller-lifecycle", selected["adapter"]["capabilities"])
        manifest = (ROOT / "apps/interactive-demo/main/idf_component.yml").read_text()
        self.assertIn('lvgl/lvgl: "==9.2.2"', manifest)

    def test_shared_bus_pins_preserve_board_recipes(self):
        for board in ("wireless/wt9932p4-tiny-v1.2", "waveshare/esp32-p4-pico"):
            config = (ROOT / "platforms/esp32p4/boards" / board / "board_config.h").read_text()
            self.assertIn("KMYC_BOARD_I2C_SCL_GPIO 8", config)
            self.assertIn("KMYC_BOARD_I2C_SDA_GPIO 7", config)
        for product, name in (("7inch/KMYC-T070-CTP-I2C-GT911-G01-A1", "gt911"),
                              ("10.1inch/KMYC-T101-CTP-I2C-GT9271-G01-A1", "gt9271")):
            driver = (ROOT / "touch" / product / "driver" / (name + "_touch.c")).read_text()
            self.assertIn("kmyc_board_acquire_i2c", driver)
            self.assertNotIn("i2c_new_master_bus", driver)
            self.assertIn("kmyc_touch_notify_wake_signal_complete", driver)

    def test_demo_events_queue_actions_without_chip_io(self):
        ui = (ROOT / "apps/interactive-demo/main/demo_ui.c").read_text()
        service = (ROOT / "apps/interactive-demo/main/demo_service.c").read_text()
        main = (ROOT / "apps/interactive-demo/main/main.c").read_text()
        event = ui.split("static void action(", 1)[1].split("static lv_obj_t *button", 1)[0]
        self.assertIn("demo_service_queue_action", event)
        self.assertIn("xQueueSend", service)
        self.assertNotIn("lvgl.h", service)
        self.assertNotIn("kmyc_touch_read", ui)
        self.assertNotIn("lv_mem_add_pool", ui)
        self.assertIn("MALLOC_CAP_SPIRAM", ui)
        self.assertLess(len(main.splitlines()), 30)
        self.assertNotIn("kmyc_display_sleep", event)
        self.assertNotIn("kmyc_touch_", event)
        self.assertIn("on_color_trans_done", (ROOT / "platforms/esp32p4/adapters/common/dsi2_diagnostic.c").read_text())

    def test_touch_markers_use_parent_content_coordinates(self):
        ui = (ROOT / "apps/interactive-demo/main/demo_ui.c").read_text()
        poll = ui.split("static void poll_touch(void)", 1)[1].split("static void lvgl_task", 1)[0]
        self.assertIn("lv_obj_update_layout(s_touch_area)", poll)
        self.assertLess(poll.index("lv_obj_update_layout"), poll.index("lv_obj_get_content_coords"))
        self.assertIn("lv_obj_get_content_coords(s_touch_area, &content)", poll)
        self.assertNotIn("lv_obj_get_coords(s_touch_area", poll)
        for axis in ("x", "y"):
            self.assertIn(f"s_report.points[i].{axis} >= content.{axis}1", poll)
            self.assertIn(f"s_report.points[i].{axis} <= content.{axis}2", poll)
            self.assertIn(f"s_report.points[i].{axis} - content.{axis}1 -", poll)
        self.assertIn("lv_obj_get_width(s_points[i]) / 2", poll)
        self.assertIn("lv_obj_get_height(s_points[i]) / 2", poll)
        self.assertIn("lv_obj_align(s_points[i], LV_ALIGN_TOP_LEFT, 0, 0)", ui)
        self.assertIn("lv_obj_remove_flag(s_touch_area, LV_OBJ_FLAG_SCROLLABLE)", ui)
        self.assertNotIn("- 6", poll)

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

    def test_waveshare_pico_uses_selected_silicon_profile(self):
        preset = copy.deepcopy(self.data["preset"][WAVESHARE_PRESET])
        selected = kmyc.resolve(self.data, preset)
        self.assertEqual(selected["board"]["manufacturer"], "waveshare")
        self.assertEqual(selected["board"]["model"], "ESP32-P4-Pico")
        self.assertEqual(preset["board_profile"], "rev3_x")
        defaults = kmyc.default_files(ROOT, selected)
        self.assertTrue(any(path.name == "sdkconfig.rev3_x.defaults" for path in defaults))

    def test_waveshare_pico_requires_known_silicon_profile(self):
        preset = copy.deepcopy(self.data["preset"][WAVESHARE_PRESET])
        preset.pop("board_profile")
        with self.assertRaisesRegex(ValueError, "board_profile"):
            kmyc.resolve(self.data, preset)

    def test_preset_can_select_a_diagnostic_config(self):
        preset = copy.deepcopy(self.data["preset"][WAVESHARE_PANEL_PRESET])
        selected = kmyc.resolve(self.data, preset)
        defaults = kmyc.default_files(ROOT, selected)
        self.assertEqual(defaults[-1].name, "panel-hw-colorbar.defaults")
        self.assertEqual(preset["required_config"]["CONFIG_KMYC_PANEL_INTERNAL_BIST"], "n")

    def test_touch_app_selects_implemented_assembly_and_own_main(self):
        preset = copy.deepcopy(self.data["preset"][WAVESHARE_TOUCH_PRESET])
        selected = kmyc.resolve(self.data, preset)
        self.assertEqual(selected["touch"]["controller"], "GT9271")
        self.assertEqual(selected["assembly"]["model"], preset["assembly"])
        self.assertIn("rgb888-draw", selected["app"]["requires"])
        self.assertIn("rgb888-draw", selected["adapter"]["capabilities"])
        self.assertIn("touch-poll", selected["adapter"]["capabilities"])
        self.assertTrue(kmyc.source_paths(ROOT, selected, "sources", ("touch",)))
        self.assertTrue((ROOT / "apps/touch-test/main/main.c").is_file())
        preset["board_profile"] = "unknown"
        with self.assertRaisesRegex(ValueError, "board_profile"):
            kmyc.resolve(self.data, preset)

    def test_standalone_touch_does_not_invent_an_assembly(self):
        preset = copy.deepcopy(self.data["preset"][D070_TOUCH_PRESET])
        selected = kmyc.resolve(self.data, preset)
        self.assertNotIn("assembly", selected)
        self.assertEqual(selected["display"]["controller"], "JD9165BA")
        self.assertEqual(selected["touch"]["controller"], "GT911")
        self.assertEqual(selected["adapter"]["mode"], "dsi2-750mbps-51mhz")

    def test_wireless_board_reuses_the_d070_products(self):
        preset = copy.deepcopy(self.data["preset"][WIRELESS_D070_TOUCH_PRESET])
        selected = kmyc.resolve(self.data, preset)
        self.assertEqual(selected["board"]["model"], "WT9932P4-TINY_1V2")
        self.assertEqual(selected["display"]["controller"], "JD9165BA")
        self.assertEqual(selected["touch"]["controller"], "GT911")
        self.assertEqual(selected["adapter"]["mode"], "dsi2-750mbps-51mhz")

    def test_preset_list_is_human_readable(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            kmyc.print_preset_list(self.data)
        text = output.getvalue()
        self.assertIn("waveshare-pico-r1-d070-touch", text)
        self.assertIn("App: touch-test", text)
        self.assertIn("Board: waveshare ESP32-P4-Pico (rev1_3)", text)
        self.assertIn("Touch: KMYC-T101-CTP-I2C-GT9271-G01-A1", text)

    def test_catalog_does_not_duplicate_compatibility_status(self):
        def check(value):
            if isinstance(value, dict):
                self.assertNotIn("status", value)
                for child in value.values():
                    check(child)
            elif isinstance(value, list):
                for child in value:
                    check(child)
            elif isinstance(value, str):
                self.assertNotIn("experimental", value.lower())

        check(self.data)
        self.assertTrue((ROOT / "docs/hardware-support.md").is_file())

    def test_rev3_profile_accepts_kconfig_unset_syntax(self):
        preset = copy.deepcopy(self.data["preset"][WAVESHARE_PRESET])
        selected = kmyc.resolve(self.data, preset)
        required = dict(selected["board"]["required_config"])
        required.update(selected["board"]["_selected_profile"]["required_config"])
        with tempfile.TemporaryDirectory(prefix="kmyc-rev3-") as directory:
            path = Path(directory) / "sdkconfig"
            lines = ['CONFIG_IDF_TARGET="esp32p4"']
            for key, value in required.items():
                lines.append(f"# {key} is not set" if value == "n" else f"{key}={value}")
            path.write_text("\n".join(lines), encoding="utf-8")
            kmyc.verify_sdkconfig(selected, path)
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
