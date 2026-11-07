#!/usr/bin/env python3

from pathlib import Path
import subprocess
import sys

# Project directory anchors the build regardless of the caller's working directory.
# Resolving the script path may raise OSError.
# Каталог проекта задаёт путь сборки независимо от рабочего каталога вызова.
# Разрешение пути скрипта может вызвать OSError.
PROJECT_ROOT = Path(__file__).resolve().parent
# Build directory relative to the project. No errors or exceptions.
# Каталог сборки относительно проекта. Ошибок и исключений нет.
BUILD_DIR = "build"
# Meson's test log relative to the build. No errors or exceptions.
# Журнал тестов Meson относительно сборки. Ошибок и исключений нет.
TEST_LOG = Path("meson-logs") / "testlog.txt"
# Summary marker emitted by the C++ test runner. No errors or exceptions.
# Метка итогов, выводимая запускателем тестов C++. Ошибок и исключений нет.
SUMMARY_MARKER = "tests passed"


# Runs Meson and prints suite summaries; returns its failing status or 0 after success.
# I/O failures return 1 with a diagnostic; MemoryError and KeyboardInterrupt propagate.
# Запускает Meson и печатает итоги наборов; возвращает код сбоя или 0 после успеха.
# Сбои ОС возвращают 1 с диагностикой; MemoryError и KeyboardInterrupt передаются выше.
def main():
    try:
        result = subprocess.run(
            ["meson", "test", "-C", BUILD_DIR, "--print-errorlogs"], cwd=PROJECT_ROOT)

        # Preserve Meson's failure status without presenting a successful summary.
        # Сохраняем код сбоя Meson без вывода успешной сводки.
        if result.returncode:
            return result.returncode

        with (PROJECT_ROOT / BUILD_DIR / TEST_LOG).open(encoding="utf-8", errors="replace") as log:
            # Select summaries directly in Python instead of relying on grep.
            # Выбираем итоги средствами Python без зависимости от grep.
            for line in log:
                if SUMMARY_MARKER in line:
                    print(line, end="")
    except OSError as error:
        print(error, file=sys.stderr)
        return 1

    return 0


# Propagate the command result to the caller. No additional exceptions.
# Передаём результат команды вызывающему коду. Дополнительных исключений нет.
if __name__ == "__main__":
    sys.exit(main())
