# Interactive bridge demo

ESP-IDF **5.5.3**, LVGL **9.2.2**, RGB888, 1024×600. This is a public,
self-contained application for the D070 display / GT911 / bridge V1.2 recipe.
Integration is **under test**; a successful build is not physical verification.

```powershell
python tools/kmyc.py check --preset wireless-p4-d070-bridge-v12-demo
python tools/kmyc.py build --preset wireless-p4-d070-bridge-v12-demo
python -m unittest discover -s tests
```

Run those commands from the repository root in the matching IDF environment.
Windows long-path workarounds can use an unused short SUBST mapping of this same
repository with direct CMake `-S K:/apps/interactive-demo` and a short build path;
do not overwrite an existing drive or reuse a cache from another source path.
The generated dependency lock pins LVGL and must not be hand-edited.

## Connection and policy

Power off before changing MIPI/touch cables. Confirm the exact display assembly,
bridge revision, ground, supply and serial console before any flash or power-on.
The production connector bus is **SDA GPIO7 / SCL GPIO8**, not the internal
two-development-board fixture's GPIO16/17 wiring. Controller 0x2C and GT911 share
one board-owned bus for the firmware lifetime. Internal pulls preserve previous
board recipes but do not replace appropriate external bus pull resistors.
There must be only one controlling host; another master or a controller reset can
invalidate sequence-based recovery from an ambiguous write timeout.

Controller probe validates ABI/capabilities before control writes. Missing or
incompatible controllers receive no control frames: panel startup uses the prior
DCS software-reset route and touch is probed directly. Controller UI controls
remain disabled. Descriptor product mismatch is a warning only; the compiled
display/touch preset stays authoritative. No Flash/configuration write API is used.
Three consecutive runtime status failures mark the controller offline and disable
control; no automatic release, reset or command replay occurs.

The host owns all device-specific timing. LCD reset: low20ms, high then input,
wait120ms. GT911 0x5D selection: RST/INT low10ms, RST high, INT input after5ms,
wait until200ms after RST high. TP_RST stays high during operation. Sleep uses
PWM0, DCS28/20ms, DCS10/120ms, then INT low, GT911 sleep command, INT input.
Wake uses INT high3ms/input, a5ms host readiness margin, identity read (at most
one additional read after5ms), DCS11/120ms, DCS29/20ms and saved
brightness. UI sleep is bounded by an automatic wake after three seconds.
The completed wake signal clears the driver's local sleep gate before identity
confirmation; a transient identity failure therefore cannot suppress all later
touch polling. This notification does not declare the touch online. Sleep holds
INT low for a5ms setup margin before sending the sleep command. No wake pulse is
replayed and no automatic reset is used.
Startup failure after a compatible probe attempts one PWM0/LCD_RST-low safeguard.

## Tasks and UI

The example separates ownership into small entry points and two task modules:

- `main/main.c`: start the service, then start the UI.
- `main/demo_service.c/.h`: action queues, device lifecycle, polling and model snapshots.
- `main/demo_model.h`: value-only state shared across the task boundary.
- `main/demo_ui.c/.h`: LVGL objects, input presentation and flush completion.

For a new UI, keep the service API and replace the UI module. For a new board or
bridge, change platform/adapter support instead of adding pin timing to UI events.

The LVGL task (priority4, 8KiB stack) alone calls LVGL except its documented
thread-safe 2ms tick. A separate service task polls touch at20ms and status at1s,
and consumes bounded action queues. A mutex-protected model transfers snapshots.
Slider values coalesce in a one-entry queue, with service writes at most every30ms;
release submits the final value. Public brightness is0–100%, rounded to raw0–255.

Two40-line PSRAM draw buffers are allocated; UI objects use LVGL's configured
default allocator without an additional application-managed memory pool.
Draw buffers feed the DPI framebuffer. The display registers
`on_color_trans_done`; its callback only notifies the LVGL task. That task calls
`lv_display_flush_ready` after completion. A lost callback stops UI buffer reuse
instead of pretending a pending copy completed. This is not tear-free scanout.

- **System:** board and full display/touch/adapter identities, ABI/FW/HW versions,
  capabilities, raw GPIO/PWM status and diagnostic counts. Standard descriptor
  TLVs show IDs/controllers, dimensions, interface/lanes/pixel format, orientation
  and touch candidate addresses; unknown TLVs remain type/length summaries.
- **Display:** RGB bars, grayscale, checker/grid previews, percent/raw brightness,
  sleep and wake.
- **Touch:** up to five markers and id/x/y/strength values updated at20ms,
  five hit targets, maximum simultaneous contacts, edge/multi-report counters,
  GT911 identity, reset and reprobe. An unavailable touch does not abort display.
- **Lifecycle:** startup stage checklist, degraded/error state and six recent
  action/error records, diagnostic clear,
  three-second sleep/wake and explicit P4 restart.

No arbitrary-width glitch, STOP-to-edge timing, atomic/power-cut qualification,
24-hour endurance, panel color/orientation or real touch validation is claimed.
Compatibility status is maintained only in ../../docs/hardware-support.md.

LVGL interface reference: https://docs.lvgl.io/9.2/porting/display.html
