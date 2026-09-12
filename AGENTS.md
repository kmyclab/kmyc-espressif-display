# KMYC ESP-IDF workspace

This is the public KMYC Espressif display repository. Keep published source,
reproducible configuration, customer-facing documentation and validation records
here. Do not add credentials, local environment records, vendor documents, build
output or material that is not cleared for redistribution.

Read docs/architecture.md and the validation record for the selected adapter before
changing firmware. Preserve the established display, touch and assembly identities.

- Each apps/<app> is an independent C ESP-IDF project with its own app_main().
- Use four spaces, snake_case C identifiers and uppercase macros.
- Keep GPIO, power and chip HAL details outside App code.
- Product directories are display/<size>/<full model>, touch/<size>/<full model>
  and assembly/<size>/<full model>. Do not replace models with supplier parts.
- Chip-specific code belongs in platforms/<idf_target>/. Manufacturer wireless
  means 启明云端. Preserve physical revision in the board ID.
- Do not create working catalog entries for planned apps, chips or boards.
- Keep the existing BIST mode, supplier command sequence, two-lane experimental
  settings, ACK policy and SDK 5.5.3 baseline unless explicitly debugging them.
- Do not flash or request hardware changes during software restructuring.
  Before physical bring-up, require power-off and exact board/display/touch,
  adapter, serial and network confirmation. No hot-plugging FFCs.
- Do not add supplier documents or third-party source unless redistribution rights
  and customer relevance have been confirmed.
- Catalog .yaml files use JSON syntax (a YAML subset), matching the existing Pi
  catalog convention. Maintain one source list in module metadata.
- Run python tools/kmyc.py check and python -m unittest discover -s tests.
  For firmware changes, build the affected preset using its specified SDK.
- Record build checks separately from hardware/visual results in docs/validation/.
- Do not hand-edit dependencies.lock. Keep out/, managed_components/, generated
  sdkconfig, credentials, raw logs and archives out of Git.
