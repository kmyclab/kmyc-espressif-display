# KMYC-D070-DSI4L-1024X600-A1

7-inch 1024x600 LCD using the JD9165BA controller. The panel product exposes four
MIPI-DSI data lanes; the controller can also be configured for a two-lane host, which
is the mode used by the Waveshare ESP32-P4-Pico adapter.

The ESP32-P4 mode uses RGB888, a 51 MHz pixel clock and 750 Mbps per lane. Its timing
is 1024x600 with HFP 160, HSYNC 24, HBP 136, VFP 12, VSYNC 2 and VBP 21.

The original supplier PDFs, drawing and command-table file are not redistributed in
this public repository. The initialization was ported from KMYC's public Raspberry Pi
driver. For the two-lane adapter it applies the controller's lane-select value and
enables software lane selection.
