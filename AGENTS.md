# Repository instructions

This is the public KMYC Espressif display repository. Published firmware must be
self-contained and must not depend on files from a private parent workspace. Never
commit credentials, customer data, supplier documents, raw logs or generated output.

- Each `apps/<app>/` is an independent C ESP-IDF project with its own
  `main/main.c` and `app_main()`.
- Use four-space indentation, `snake_case` C identifiers and uppercase macros.
- Keep GPIO, power control and chip HAL details outside application code.
- Keep product identities under `display/`, `touch/` and `assembly/`; chip and board
  support belongs under `platforms/<idf_target>/`.
- Use `wireless` as the manufacturer ID for 启明云端, including
  WT9932P4-TINY V1.2.
- Catalog `.yaml` files use JSON syntax. Register only implemented combinations and
  keep source lists in module metadata.
- Maintain compatibility status only in `docs/hardware-support.md`. Do not create
  per-adapter status or validation-report files.
- Before changing connected hardware, power off the board and display. Never
  hot-plug MIPI-DSI or touch FFCs.
- Run `python tools/kmyc.py check` and `python -m unittest discover -s tests` before
  committing. Build affected firmware with its preset and declared ESP-IDF version.
- Do not hand-edit `dependencies.lock`. Keep `out/`, `build/`,
  `managed_components/`, generated `sdkconfig`, local archives and logs out of Git.
