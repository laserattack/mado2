#!/usr/bin/env python3

from pathlib import Path
import subprocess
import sys

PROJECT_ROOT = Path(__file__).resolve().parent
BUILD_DIR = "build"
TEST_LOG = Path("meson-logs") / "testlog.txt"
SUMMARY_MARKER = "tests passed"


def main():
    try:
        result = subprocess.run(
            ["meson", "test", "-C", BUILD_DIR, "--print-errorlogs"], cwd=PROJECT_ROOT)

        if result.returncode:
            return result.returncode

        with (PROJECT_ROOT / BUILD_DIR / TEST_LOG).open(encoding="utf-8", errors="replace") as log:
            for line in log:
                if SUMMARY_MARKER in line:
                    print(line, end="")
    except OSError as error:
        print(error, file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
