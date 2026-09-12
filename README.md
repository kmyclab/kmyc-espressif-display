# KMYC Espressif Display

KMYC display software for the Espressif ecosystem.

## Repository status

The repository publishes an experimental ESP-IDF 5.5.3 panel diagnostic for the
ESP32-P4. It currently targets the Wireless WT9932P4-TINY V1.2 and the
KMYC-D101-DSI4L-800X1280-A1 display in a two-lane diagnostic configuration.

The source is organized as independent C applications, reusable display products,
and chip-specific board support. Build success and hardware validation are tracked
separately; the current adapter remains experimental and is not a production
compatibility claim.

## Validate and build

Catalog validation and unit tests only require Python 3.9 or newer:

```sh
python tools/kmyc.py check --preset wireless-p4-d101-panel-test
python -m unittest discover -s tests
```

Build from an ESP-IDF 5.5.3 environment:

```sh
python tools/kmyc.py build --preset wireless-p4-d101-panel-test
```

See `docs/bringup.md` before flashing or changing connected hardware.

## Clone

```sh
git clone https://github.com/kmyclab/kmyc-espressif-display.git
```

This repository is self-contained and does not require files from a parent
engineering workspace.

## License

KMYC-authored source is licensed under GPL-2.0-only. Third-party ESP-IDF components
retain their respective licenses. See [LICENSE](LICENSE).
