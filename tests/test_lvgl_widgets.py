"""Contract checks for the independent upstream LVGL widgets application."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("widgets_catalog", ROOT / "tools/kmyc.py")
kmyc = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(kmyc)
PRESET = "wireless-p4-d101-bridge-v12-widgets"


class WidgetsTests(unittest.TestCase):
    def test_widgets_reuses_existing_bridge_products(self):
        data = kmyc.catalog(ROOT)
        selected = kmyc.resolve(data, data["preset"][PRESET])
        self.assertEqual(selected["app"]["id"], "lvgl-widgets")
        self.assertEqual(selected["board"]["id"], "wireless-wt9932p4-tiny-v1.2")
        self.assertEqual(selected["display"]["controller"], "ILI9881C")
        self.assertEqual(selected["touch"]["controller"], "GT9271")
        self.assertEqual(selected["adapter"]["id"], "wireless-tiny-d101-bridge-v12-dsi2")
        self.assertEqual(selected["board"]["required_config"]["CONFIG_ESP32P4_SELECTS_REV_LESS_V3"], "y")

    def test_uses_upstream_widgets_and_own_entry(self):
        app = ROOT / "apps/lvgl-widgets"
        self.assertIn("void app_main(void)", (app / "main/main.c").read_text())
        port = (app / "main/lvgl_port.c").read_text()
        self.assertIn("lv_demo_widgets();", port)
        self.assertIn("lv_display_set_flush_wait_cb", port)
        self.assertIn("vTaskNotifyGiveFromISR", port)
        self.assertIn("if (updated)", port)
        self.assertNotIn("gpio_", port)
        self.assertNotIn("interactive-demo", (app / "main/CMakeLists.txt").read_text())
        config = (app / "sdkconfig.defaults").read_text()
        for option in ("LV_USE_DEMO_WIDGETS", "LV_COLOR_DEPTH_24",
                       "LV_USE_CLIB_MALLOC", "LV_FONT_MONTSERRAT_16", "LV_FONT_MONTSERRAT_24"):
            self.assertIn(f"CONFIG_{option}=y", config)


if __name__ == "__main__":
    unittest.main()
