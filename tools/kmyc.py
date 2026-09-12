#!/usr/bin/env python3
"""Discover, validate and build independent KMYC C applications (Python 3.9+).

Product .yaml files deliberately use the JSON subset of YAML, as in the
Raspberry Pi catalog. No YAML package or SDK is needed for list/check/select.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PATTERNS = {
    "app": "apps/*/app.json",
    "platform": "platforms/*/platform.json",
    "board": "platforms/*/boards/*/*/board.json",
    "display": "display/*/*/component.yaml",
    "touch": "touch/*/*/component.yaml",
    "assembly": "assembly/*/*/assembly.yaml",
    "adapter": "platforms/*/adapters/*/adapter.json",
    "preset": "presets/*.json",
}
PLURALS = {kind + "s": kind for kind in PATTERNS if kind != "assembly"}
PLURALS["assemblies"] = "assembly"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def inside(root, path):
    root, path = root.resolve(), path.resolve()
    require(path == root or root in path.parents, f"Path escapes workspace: {path}")
    return path


def catalog(root=ROOT):
    result = {kind: {} for kind in PATTERNS}
    for kind, pattern in PATTERNS.items():
        for path in sorted(root.glob(pattern)):
            entry = read_json(path)
            require(entry.get("schema") == 1 and entry.get("kind") == kind,
                    f"Invalid schema/kind: {path}")
            key = entry.get("model") if kind in ("display", "touch", "assembly") else entry.get("id")
            require(isinstance(key, str) and key, f"Missing identity: {path}")
            require(key not in result[kind], f"Duplicate {kind}: {key}")
            if kind in ("display", "touch", "assembly"):
                prefix = {"display": "D", "touch": "T", "assembly": "DT"}[kind]
                require(re.fullmatch(r"KMYC-" + prefix + r"\d{3}-[A-Z0-9-]+-[A-Z]\d+", key),
                        f"Invalid KMYC product identity: {key}")
                require(path.parent.name == key, f"Product directory must equal full model: {path}")
                require(re.fullmatch(r"\d+(?:\.\d+)?inch", path.parent.parent.name),
                        f"Missing product size directory: {path}")
                for name in ("README.md",) if kind == "assembly" else ("README.md", "VERSION", "CHANGELOG.md"):
                    require((path.parent / name).is_file(), f"Missing product document: {path.parent / name}")
                if kind != "assembly":
                    require((path.parent / "VERSION").read_text().strip() == entry["version"],
                            f"VERSION and component metadata disagree: {key}")
            else:
                require(re.fullmatch(r"[a-z0-9][a-z0-9.-]*", key), f"Invalid software ID: {key}")
            entry["_path"] = inside(root, path)
            result[kind][key] = entry
    for entry in result["assembly"].values():
        require(entry["display"] in result["display"] and entry["touch"] in result["touch"],
                f"Unresolved assembly references: {entry['model']}")
    for entry in result["board"].values():
        path = entry["_path"]
        require(path.parent.parent.name == entry["manufacturer"], f"Manufacturer/path mismatch: {path}")
        require(path.parents[3].name == entry["target"], f"Target/path mismatch: {path}")
    return result


def lookup(data, kind, key):
    require(key in data[kind], f"Unknown or not implemented {kind}: {key}")
    return data[kind][key]


def resolve(data, preset):
    app = lookup(data, "app", preset["app"])
    board = lookup(data, "board", preset["board"])
    platform = lookup(data, "platform", board["target"])
    display = lookup(data, "display", preset["display"])
    adapter = lookup(data, "adapter", preset["adapter"])
    require(board["target"] in app["targets"], "App does not support this chip")
    require(adapter["board"] == board["id"] and adapter["display"] == display["model"]
            and adapter["target"] == board["target"], "Adapter does not match board/display/target")
    require(adapter["mode"] in {mode["id"] for mode in display["modes"]},
            "Adapter references an unsupported display mode")
    require(set(app["requires"]).issubset(adapter["capabilities"]),
            "Adapter lacks capabilities required by the App")
    require(preset["idf"] in platform["idf_versions"], "SDK version is not registered for this platform")
    if preset.get("assembly"):
        assembly = lookup(data, "assembly", preset["assembly"])
        require(assembly["display"] == display["model"], "Assembly/display mismatch")
        require(assembly["model"] in adapter["assemblies"],
                "Assembly is metadata-only or has no implemented adapter")
    return {"preset": preset, "app": app, "board": board, "platform": platform,
            "display": display, "adapter": adapter}


def source_paths(root, selection, field, kinds=("platform", "board", "display", "adapter")):
    paths = []
    for kind in kinds:
        entry = selection[kind]
        for value in entry.get(field, []):
            path = inside(root, entry["_path"].parent / value)
            require(path.is_file() if field == "sources" else path.is_dir(), f"Missing {field}: {path}")
            paths.append(path)
    return paths


def default_files(root, selection):
    return [root / "sdkconfig.defaults"] + [
        selection[kind]["_path"].parent / "sdkconfig.defaults"
        for kind in ("platform", "board", "app")
    ]


def fingerprint(root, selection):
    # Changing the hardware recipe must not silently reuse an old sdkconfig.
    inputs = [entry["_path"] for entry in selection.values()] + default_files(root, selection)
    return hashlib.sha256(b"\0".join(path.read_bytes() for path in inputs)).hexdigest()


def write_changed(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        with path.open("w", encoding="utf-8", newline="\n") as output:
            output.write(content)


def cmake_set(name, values):
    # Bracket arguments preserve Windows paths and avoid CMake interpolation.
    require(all("]==]" not in str(value) and ";" not in str(value) for value in values),
            "Unsupported CMake path/value")
    return "set(" + name + "\n" + "".join(f"  [==[{value}]==]\n" for value in values) + ")\n"


def configure(root, selection, output):
    output = inside(root / "out", output)
    sources = source_paths(root, selection, "sources", ("platform", "board", "adapter"))
    includes = source_paths(root, selection, "include_dirs", ("platform", "board", "adapter"))
    product_sources = source_paths(root, selection, "sources", ("display",))
    product_includes = source_paths(root, selection, "include_dirs", ("display",))
    defaults = default_files(root, selection)
    for path in defaults:
        require(path.is_file(), f"Missing defaults: {path}")
    stamp = fingerprint(root, selection)
    record_path = output / "selection.json"
    if record_path.exists() and (output / "sdkconfig").exists():
        require(read_json(record_path)["fingerprint"] == stamp,
                "Recipe changed since sdkconfig was created. Use a new preset ID or archive "
                "this preset's out directory before configuring again.")
    generated = output / "generated"
    preset = selection["preset"]
    body = "# Generated by tools/kmyc.py; do not edit.\n"
    body += cmake_set("KMYC_DISPLAY_SOURCES", [path.as_posix() for path in sources])
    body += cmake_set("KMYC_DISPLAY_INCLUDE_DIRS", [path.as_posix() for path in includes])
    body += cmake_set("KMYC_PRODUCT_SOURCES", [path.as_posix() for path in product_sources])
    body += cmake_set("KMYC_PRODUCT_INCLUDE_DIRS", [path.as_posix() for path in product_includes])
    # A selected display exposes the short IDF component name kmyc_panel.
    # This keeps product identities out of Windows object/library filenames.
    product_dir = selection["display"]["_path"].parent / "driver/kmyc_panel"
    require((product_dir / "CMakeLists.txt").is_file(), "Missing product IDF component")
    body += cmake_set("KMYC_PRODUCT_COMPONENT", ["kmyc_panel"])
    body += cmake_set("KMYC_PRODUCT_DIR", [product_dir.as_posix()])
    body += cmake_set("KMYC_DEFAULT_FILES", [path.as_posix() for path in defaults])
    body += cmake_set("KMYC_TARGET", [selection["board"]["target"]])
    body += cmake_set("KMYC_REQUIRED_IDF", [preset["idf"]])
    body += cmake_set("KMYC_APP_ID", [preset["app"]])
    inputs = [entry["_path"].as_posix() for entry in selection.values()]
    body += cmake_set("KMYC_CATALOG_INPUTS", inputs)
    write_changed(generated / "selection.cmake", body)
    record = {key: value for key, value in preset.items() if not key.startswith("_")}
    record.update(target=selection["board"]["target"], fingerprint=stamp)
    write_changed(record_path, json.dumps(record, ensure_ascii=False, indent=2) + "\n")
    return output


def verify_sdkconfig(selection, path):
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    require(values.get("CONFIG_IDF_TARGET") == json.dumps(selection["board"]["target"]),
            "sdkconfig belongs to another target")
    for key, value in selection["board"].get("required_config", {}).items():
        require(values.get(key) == value, f"Board requires {key}={value}; current sdkconfig differs")


def run_idf(root, selection, action):
    idf_path = os.environ.get("IDF_PATH")
    require(idf_path, "Open the matching ESP-IDF PowerShell first (IDF_PATH is unset)")
    idf = Path(idf_path)
    text = (idf / "tools/cmake/version.cmake").read_text(encoding="utf-8")
    version = ".".join(re.search(r"set\(IDF_VERSION_" + part + r"\s+(\d+)\)", text)[1]
                       for part in ("MAJOR", "MINOR", "PATCH"))
    require(version == selection["preset"]["idf"],
            f"Preset requires IDF {selection['preset']['idf']}; active SDK is {version}")
    output = configure(root, selection, root / "out" / selection["preset"]["id"])
    project = selection["app"]["_path"].parent
    command = [sys.executable, str(idf / "tools/idf.py"), "-C", str(project),
               "-B", str(output / "build"),
               "-D", "KMYC_PRESET=" + selection["preset"]["id"],
               "-D", "IDF_TARGET=" + selection["board"]["target"],
               "-D", "SDKCONFIG=" + (output / "sdkconfig").as_posix(), action]
    print("Building/configuring:", selection["preset"]["id"], flush=True)
    subprocess.run(command, check=True)
    if action in ("build", "reconfigure"):
        verify_sdkconfig(selection, output / "sdkconfig")
    if action == "build":
        record = read_json(output / "selection.json")
        record["sdkconfig_sha256"] = hashlib.sha256((output / "sdkconfig").read_bytes()).hexdigest()
        record["lock_sha256"] = hashlib.sha256((project / "dependencies.lock").read_bytes()).hexdigest()
        commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, capture_output=True, text=True)
        record["workspace_commit"] = commit.stdout.strip() if commit.returncode == 0 else None
        state = subprocess.run(["git", "status", "--porcelain", "--", str(root)],
                               cwd=root, capture_output=True, text=True)
        record["source_dirty"] = bool(state.stdout.strip()) if state.returncode == 0 else None
        write_changed(output / "build-info.json", json.dumps(record, indent=2) + "\n")


def select(data):
    candidates = []
    for preset in data["preset"].values():
        candidates.append(resolve(data, preset))
    require(candidates, "No implemented build combinations")
    for field, label in (("app", "Application"), ("board", "Board"), ("display", "Display")):
        options = sorted({item["preset"][field] for item in candidates})
        print(label + ":")
        for index, option in enumerate(options, 1):
            print(f"  {index}. {option}")
        answer = input("Choose [1]: ").strip() or "1"
        require(answer.isdigit() and 1 <= int(answer) <= len(options), "Invalid selection")
        chosen = options[int(answer) - 1]
        candidates = [item for item in candidates if item["preset"][field] == chosen]
    for index, item in enumerate(candidates, 1):
        preset = item["preset"]
        print(f"  {index}. {preset['id']} | adapter={preset['adapter']} | {preset['status']}")
    answer = input("Preset [1]: ").strip() or "1"
    require(answer.isdigit() and 1 <= int(answer) <= len(candidates), "Invalid preset")
    preset = candidates[int(answer) - 1]["preset"]
    print(f"Selected: {preset['id']}\npython tools/kmyc.py build --preset {preset['id']}")
    print("Selection only; no build or flash was started.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    listing = sub.add_parser("list", help="List registered items, including metadata-only products")
    listing.add_argument("kind", choices=sorted(PLURALS))
    sub.add_parser("select", help="Interactively choose an implemented app/board/display preset")
    for name in ("check", "configure", "build", "menuconfig"):
        child = sub.add_parser(name)
        child.add_argument("--preset", required=name != "check")
        if name == "configure":
            child.add_argument("--app", help="Reject a preset for another App (used by CMake)")
    verify = sub.add_parser("verify-sdkconfig", help=argparse.SUPPRESS)
    verify.add_argument("--preset", required=True)
    verify.add_argument("--file", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        data = catalog()
        if args.action == "list":
            for key, entry in data[PLURALS[args.kind]].items():
                print(f"{key}\t{entry.get('status', 'registered')}")
        elif args.action == "select":
            select(data)
        else:
            presets = [lookup(data, "preset", args.preset)] if args.preset else list(data["preset"].values())
            for preset in presets:
                selection = resolve(data, preset)
                source_paths(ROOT, selection, "sources")
                source_paths(ROOT, selection, "include_dirs")
                if args.action == "check":
                    fingerprint(ROOT, selection)
                    print(f"OK {preset['id']} ({selection['board']['target']}, IDF {preset['idf']}, {preset['status']})")
                elif args.action == "configure":
                    require(not args.app or args.app == preset["app"], "Preset belongs to a different App")
                    print(configure(ROOT, selection, ROOT / "out" / preset["id"]))
                elif args.action == "verify-sdkconfig":
                    verify_sdkconfig(selection, args.file)
                else:
                    run_idf(ROOT, selection, args.action)
        return 0
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        print(f"KMYC: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
