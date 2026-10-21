"""Host-mock actual lifecycle function bodies; no hardware or ESP-IDF required.

Compile the named production functions unchanged with transport/time stubs. This
tests control flow, profile timing and state, not physical device readiness.
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
#include <limits.h>
#include "kmyc_touch.h"
#include "bridge_profile.h"
#include "touch_wiring.h"
#include "demo_geometry.h"
#define GT911_PRODUCT_ID_REG 0x8140
#define GT911_REPORT_STATUS_REG 0x814E
#define GT911_FIRST_POINT_REG 0x814F
#define GT911_POINT_BYTES 8
#define GT9271_PRODUCT_ID_REG GT911_PRODUCT_ID_REG
#define GT9271_REPORT_STATUS_REG GT911_REPORT_STATUS_REG
#define GT9271_FIRST_POINT_REG GT911_FIRST_POINT_REG
#define GT9271_POINT_BYTES GT911_POINT_BYTES
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
static bool sleeping, resetting, wrong_identity;
static int64_t s_sleep_command_at = -1;
static int64_t esp_timer_get_time(void) { return (int64_t)elapsed * 1000; }
static esp_err_t record(int error, const char *phase) { (void)phase; return error; }
const kmyc_touch_info_t *kmyc_touch_get_info(void) { return &s_info; }
esp_err_t kmyc_touch_start(void) { assert(0); return ESP_FAIL; }
static void vTaskDelay(unsigned ms) { elapsed += ms; }
static esp_err_t apply(uint8_t mask, uint8_t value, uint8_t mode_mask,
                       uint8_t mode, bool pwm, uint8_t duty)
{
    ++apply_calls;
    if (resetting) {
        assert(!pwm && duty == 0);
        if (apply_calls == 1) {
            assert(mask == 6 && value == 0 && mode_mask == 6 && mode == 0x14);
        } else if (apply_calls == 2) {
            assert(mask == 2 && value == 2 && mode_mask == 0);
            assert(elapsed == KMYC_BRIDGE_TOUCH_RESET_LOW_MS);
        } else {
            assert(apply_calls == 3 && mask == 0 && mode_mask == 4 && mode == 0);
            assert(elapsed == KMYC_BRIDGE_TOUCH_RESET_LOW_MS + KMYC_BRIDGE_TOUCH_RESET_HIGH_MS);
        }
        return fail_apply == apply_calls ? ESP_FAIL : ESP_OK;
    }
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
        memcpy(data, TEST_PRODUCT_ID, strlen(TEST_PRODUCT_ID));
        if (wrong_identity) data[0]='?';
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
    resetting=false;
    wrong_identity=false;
    s_sleep_command_at=-1;
}
int main(void)
{
    assert(KMYC_BRIDGE_TOUCH_SLEEP_MIN_MS == TEST_SLEEP_MIN_MS);
    assert(KMYC_BRIDGE_TOUCH_RESET_LOW_MS == TEST_RESET_LOW_MS);
    assert(KMYC_BRIDGE_TOUCH_RESET_HIGH_MS == TEST_RESET_HIGH_MS);
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
    reset_fixture(); fail_apply=1; s_sleep_command_at=0; elapsed=100;
    assert(kmyc_bridge_touch_wake() != ESP_OK);
    assert(apply_calls == 1 && reads == 0 && s_info.asleep);
    assert(s_sleep_command_at == 0);
    reset_fixture(); fail_apply=2; s_sleep_command_at=0; elapsed=100;
    assert(kmyc_bridge_touch_wake() != ESP_OK);
    assert(apply_calls == 2 && reads == 0 && s_info.asleep);
    assert(s_sleep_command_at == 0);
    reset_fixture(); sleeping=true; s_info.asleep=false;
    assert(kmyc_bridge_touch_sleep() == ESP_OK);
    assert(writes == 1 && apply_calls == 2 && elapsed == 6 && s_info.asleep);
    reset_fixture(); sleeping=true; s_info.asleep=false; fail_write=1;
    assert(kmyc_bridge_touch_sleep() != ESP_OK);
    assert(writes == 1 && apply_calls == 2 && !s_info.asleep);
    unsigned failed_command_done=elapsed;
    sleeping=false; apply_calls=0; fail_write=0;
    assert(kmyc_bridge_touch_wake() == ESP_OK);
    assert(pulse_at - failed_command_done >= KMYC_BRIDGE_TOUCH_SLEEP_MIN_MS);
    assert(s_sleep_command_at == -1);
    reset_fixture(); sleeping=true; s_info.asleep=false;
    assert(kmyc_bridge_touch_sleep() == ESP_OK);
    unsigned command_done = elapsed;
    sleeping=false; apply_calls=0;
    assert(kmyc_bridge_touch_wake() == ESP_OK);
    assert(pulse_at - command_done >= KMYC_BRIDGE_TOUCH_SLEEP_MIN_MS);
    assert(s_sleep_command_at == -1);
    assert(released_at - pulse_at == KMYC_BRIDGE_TOUCH_WAKE_HIGH_MS);
    reset_fixture(); sleeping=true; s_info.asleep=false;
    assert(kmyc_bridge_touch_sleep() == ESP_OK);
    elapsed += 100;
    command_done=elapsed; sleeping=false; apply_calls=0;
    assert(kmyc_bridge_touch_wake() == ESP_OK);
    assert(pulse_at == command_done); /* Already waited: no extra hold. */
    reset_fixture(); sleeping=true; s_info.asleep=false;
    assert(kmyc_bridge_touch_sleep() == ESP_OK);
    command_done=elapsed;
    elapsed += 58;
    sleeping=false; apply_calls=0;
    assert(kmyc_bridge_touch_wake() == ESP_OK);
    assert(pulse_at - command_done >= TEST_SLEEP_MIN_MS);
    reset_fixture(); wrong_identity=true;
    assert(kmyc_bridge_touch_wake() != ESP_OK);
    assert(reads == 2 && apply_calls == 2 && !s_info.asleep && !s_info.available);
    reset_fixture(); sleeping=true; s_info.asleep=false; fail_apply=2;
    assert(kmyc_bridge_touch_sleep() != ESP_OK);
    assert(s_info.asleep && writes == 1); /* Sleep succeeded, release failed. */
    reset_fixture(); sleeping=true; s_info.asleep=false; fail_apply=1;
    assert(kmyc_bridge_touch_sleep() != ESP_OK);
    assert(!s_info.asleep && writes == 0);
    reset_fixture(); resetting=true;
    assert(touch_reset_sequence() == ESP_OK);
    assert(apply_calls == 3 && elapsed == KMYC_BRIDGE_TOUCH_RESET_LOW_MS + 200);
    for (int failure=1; failure<=3; failure++) {
        reset_fixture(); resetting=true; fail_apply=failure;
        assert(touch_reset_sequence() != ESP_OK);
        assert(apply_calls == failure);
    }
    size_t bytes;
    assert(demo_buffer_size(1024,600,&bytes) && bytes == 122880);
    assert(demo_buffer_size(800,1280,&bytes) && bytes == 96000);
    assert(!demo_buffer_size(0,600,&bytes));
    assert(!demo_buffer_size(800,-1,&bytes));
    assert(!demo_buffer_size(INT_MAX,1280,&bytes));
    assert(!demo_buffer_size(800,1280,NULL));
    assert(!demo_point_is_edge(512,300,1024,600));
    assert(demo_point_is_edge(1023,300,1024,600));
    assert(!demo_point_is_edge(400,640,800,1280));
    assert(demo_point_is_edge(400,1279,800,1280));
    uint16_t x,y;
    transform_point(0,0,&x,&y);
#if TEST_SLEEP_MIN_MS
    assert(x == 799 && y == 0);
    transform_point(1279,799,&x,&y);
    assert(x == 0 && y == 1279);
#else
    assert(x == 0 && y == 0);
    transform_point(1023,599,&x,&y);
    assert(x == 1023 && y == 599);
#endif
    puts(KMYC_BRIDGE_TOUCH_CONTROLLER " lifecycle/profile + UI geometry host-mock PASS");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", nargs="+", required=True)
    args = parser.parse_args()
    for size, product, controller, short_id in (
        ("7inch", "KMYC-T070-CTP-I2C-GT911-G01-A1", "gt911", "d070"),
        ("10.1inch", "KMYC-T101-CTP-I2C-GT9271-G01-A1", "gt9271", "d101"),
    ):
        run_case(args.cc, size, product, controller, short_id)


def run_case(compiler, size, product, controller, short_id):
    driver = (ROOT / "touch" / size / product / "driver" / f"{controller}_touch.c").read_text()
    bridge = (ROOT / "platforms/esp32p4/adapters/common/bridge_v12.c").read_text()
    bodies = "".join(function(driver, name) for name in (
        "transform_point", "kmyc_touch_reprobe", "kmyc_touch_enter_sleep",
        "kmyc_touch_notify_wake_signal_complete", "kmyc_touch_read"))
    bodies += "".join(function(bridge, name) for name in (
        "touch_reset_sequence", "kmyc_bridge_touch_sleep", "kmyc_bridge_touch_wake"))
    with tempfile.TemporaryDirectory(prefix="kmyc-touch-") as directory:
        output = Path(directory)
        (output / "esp_err.h").write_text(
            "typedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n"
            "#define ESP_ERR_INVALID_STATE -2\n#define ESP_ERR_INVALID_ARG -3\n"
            "#define ESP_ERR_INVALID_RESPONSE -4\n")
        limits = (59, 20, 6) if short_id == "d101" else (0, 10, 5)
        (output / "test.c").write_text(
            f'#define TEST_PRODUCT_ID "{controller[2:]}"\n'
            f'#define TEST_SLEEP_MIN_MS {limits[0]}\n'
            f'#define TEST_RESET_LOW_MS {limits[1]}\n'
            f'#define TEST_RESET_HIGH_MS {limits[2]}\n' + PREAMBLE + bodies + CASES)
        executable = output / "test.exe"
        subprocess.run(compiler + ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
            "-I", str(output), "-I", str(ROOT / "components/kmyc_touch/include"),
            "-I", str(ROOT / "apps/interactive-demo/main"),
            "-I", str(ROOT / "platforms/esp32p4/boards/wireless/wt9932p4-tiny-v1.2"),
            "-I", str(ROOT / f"platforms/esp32p4/adapters/wireless-tiny-{short_id}-bridge-v12-dsi2"),
            str(output / "test.c"), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
