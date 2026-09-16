"""Host-mock actual lifecycle function bodies; no hardware or ESP-IDF required.

Compile the named production functions unchanged with transport/time stubs. This
tests control flow and state, not electrical timings or real GT911 readiness.
"""

import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = source.index("esp_err_t " + name + "(") if "esp_err_t " + name + "(" in source else source.index("void " + name + "(")
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


PREAMBLE = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "kmyc_touch.h"
#define GT911_PRODUCT_ID_REG 0x8140
#define GT911_REPORT_STATUS_REG 0x814E
#define GT911_FIRST_POINT_REG 0x814F
#define GT911_POINT_BYTES 8
#define pdMS_TO_TICKS(ms) (ms)
static void test_log(const char *format, ...) { (void)format; }
static const char *esp_err_to_name(int error) { return error ? "error" : "OK"; }
#define ESP_LOGI(tag,...) test_log(__VA_ARGS__)
#define ESP_LOGE(tag,...) test_log(__VA_ARGS__)
#define ESP_RETURN_ON_FALSE(c,e,...) do { if (!(c)) return (e); } while (0)
#define ESP_RETURN_ON_ERROR(c,...) do { int e_=(c); if(e_) return e_; } while (0)
static kmyc_touch_info_t s_info;
static void *s_device = (void *)1;
static int apply_calls, fail_apply, reads, fail_reads, writes, fail_write;
static unsigned elapsed, identity_at[2], pulse_at, released_at;
static bool sleeping;
static esp_err_t record(int error, const char *phase) { (void)phase; return error; }
const kmyc_touch_info_t *kmyc_touch_get_info(void) { return &s_info; }
esp_err_t kmyc_touch_start(void) { assert(0); return ESP_FAIL; }
static void vTaskDelay(unsigned ms) { elapsed += ms; }
static esp_err_t apply(uint8_t mask, uint8_t value, uint8_t mode_mask,
                       uint8_t mode, bool pwm, uint8_t duty)
{
    ++apply_calls;
    assert(!pwm && duty == 0 && mode_mask == 4);
    if (apply_calls == 1) {
        assert(mask == 4 && mode == 0x10 && value == (sleeping ? 0 : 4));
        pulse_at = elapsed;
    } else {
        assert(apply_calls == 2 && mask == 0 && mode == 0 && value == 0);
        released_at = elapsed;
    }
    return fail_apply == apply_calls ? ESP_FAIL : ESP_OK;
}
static esp_err_t read_register(uint16_t reg, uint8_t *data, size_t length)
{
    assert(!s_info.asleep); /* A completed pulse must permit real bus access. */
    ++reads;
    memset(data, 0, length);
    if (reg == GT911_PRODUCT_ID_REG) {
        assert(reads <= 2);
        identity_at[reads - 1] = elapsed;
        if (reads <= fail_reads) return ESP_FAIL;
        assert(length == 6);
        memcpy(data, "911", 3);
        data[4] = 0x60;
        data[5] = 0x10;
    } else {
        assert(reg == GT911_REPORT_STATUS_REG && length == 1);
    }
    return ESP_OK;
}
static esp_err_t write_u8(uint16_t reg, uint8_t value)
{
    ++writes;
    assert(reg == 0x8040 && value == 5 && elapsed >= 5);
    return fail_write ? ESP_FAIL : ESP_OK;
}
static void transform_point(uint16_t x, uint16_t y, uint16_t *tx, uint16_t *ty)
{
    *tx=x; *ty=y;
}
'''

CASES = r'''
static void reset_fixture(void)
{
    memset(&s_info, 0, sizeof(s_info));
    s_info.available = true;
    s_info.asleep = true;
    apply_calls=fail_apply=reads=fail_reads=writes=fail_write=0;
    elapsed=pulse_at=released_at=0;
    memset(identity_at, 0, sizeof(identity_at));
    sleeping=false;
}
int main(void)
{
    reset_fixture();
    assert(kmyc_bridge_touch_wake() == ESP_OK);
    assert(apply_calls == 2 && reads == 1 && !s_info.asleep && s_info.available);
    assert(released_at - pulse_at == 3 && identity_at[0] - released_at == 5);
    reset_fixture(); fail_reads=1;
    assert(kmyc_bridge_touch_wake() == ESP_OK);
    assert(apply_calls == 2 && reads == 2 && !s_info.asleep && s_info.available);
    assert(identity_at[1] - identity_at[0] == 5);
    reset_fixture(); fail_reads=2;
    assert(kmyc_bridge_touch_wake() != ESP_OK);
    assert(apply_calls == 2 && reads == 2 && !s_info.asleep && !s_info.available);
    kmyc_touch_report_t report;
    bool updated=true;
    assert(kmyc_touch_read(&report, &updated) == ESP_OK);
    assert(reads == 3 && !updated); /* No permanent local asleep gate. */
    reset_fixture(); fail_apply=1;
    assert(kmyc_bridge_touch_wake() != ESP_OK);
    assert(apply_calls == 1 && reads == 0 && s_info.asleep);
    reset_fixture(); fail_apply=2;
    assert(kmyc_bridge_touch_wake() != ESP_OK);
    assert(apply_calls == 2 && reads == 0 && s_info.asleep);
    reset_fixture(); sleeping=true; s_info.asleep=false;
    assert(kmyc_bridge_touch_sleep() == ESP_OK);
    assert(writes == 1 && apply_calls == 2 && elapsed == 6 && s_info.asleep);
    reset_fixture(); sleeping=true; s_info.asleep=false; fail_write=1;
    assert(kmyc_bridge_touch_sleep() != ESP_OK);
    assert(writes == 1 && apply_calls == 2 && !s_info.asleep);
    puts("GT911 lifecycle: 7 host-mock cases PASS");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", nargs="+", required=True)
    args = parser.parse_args()
    driver = (ROOT / "touch/7inch/KMYC-T070-CTP-I2C-GT911-G01-A1/driver/gt911_touch.c").read_text()
    bridge = (ROOT / "platforms/esp32p4/adapters/wireless-tiny-d070-bridge-v12-dsi2/bridge.c").read_text()
    bodies = "".join(function(driver, name) for name in (
        "kmyc_touch_reprobe", "kmyc_touch_enter_sleep",
        "kmyc_touch_notify_wake_signal_complete", "kmyc_touch_read"))
    bodies += "".join(function(bridge, name) for name in (
        "kmyc_bridge_touch_sleep", "kmyc_bridge_touch_wake"))
    with tempfile.TemporaryDirectory(prefix="kmyc-touch-") as directory:
        output = Path(directory)
        (output / "esp_err.h").write_text(
            "typedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n"
            "#define ESP_ERR_INVALID_STATE -2\n#define ESP_ERR_INVALID_ARG -3\n"
            "#define ESP_ERR_INVALID_RESPONSE -4\n")
        (output / "test.c").write_text(PREAMBLE + bodies + CASES)
        executable = output / "test.exe"
        subprocess.run(args.cc + ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
            "-I", str(output), "-I", str(ROOT / "components/kmyc_touch/include"),
            str(output / "test.c"), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
