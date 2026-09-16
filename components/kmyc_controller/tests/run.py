"""Build and run the portable C test without ESP-IDF or generated repo files."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", nargs="+",
                        help="C compiler command, e.g. --cc clang or --cc zig cc")
    args = parser.parse_args()
    compiler = args.cc
    if compiler is None:
        if os.environ.get("CC"):
            compiler = [os.environ["CC"]]
        else:
            for name in ("cc", "clang", "gcc", "zig"):
                found = shutil.which(name)
                if found:
                    compiler = [found] + (["cc"] if name == "zig" else [])
                    break
    if compiler is None:
        parser.error("No C11 compiler found on PATH. Install Clang/GCC/Zig, or pass "
                     "--cc <compiler-path> (for Zig: --cc <zig-path> cc).")
    component = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="kmyc-controller-") as directory:
        executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
        try:
            subprocess.run(compiler + ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                                       "-I", str(component / "include"),
                                       str(component / "kmyc_controller.c"),
                                       str(component / "tests" / "test_controller.c"),
                                       "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
        except OSError as error:
            print(f"Cannot execute compiler/test: {error}. Use --cc <compiler-path> "
                  "or --cc <zig-path> cc.", file=sys.stderr)
            return 2
        except subprocess.CalledProcessError as error:
            return error.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())
