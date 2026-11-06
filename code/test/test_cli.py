import json
import locale
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


# Built CLI executable used by subprocess tests.
# Собранный исполняемый файл CLI для запуска в дочернем процессе.
MADO = Path(sys.argv.pop(1)).resolve()
# Project root used for isolated, Git-ignored fixtures.
# Корень проекта для изолированных тестовых каталогов, игнорируемых Git.
PROJECT = Path(sys.argv.pop(1)).resolve()
# Expected contents of a new task without a template.
# Ожидаемое содержимое задачи без шаблона.
TASK = "- NAME:\n- PRIORITY:\n- TAGS:\n- STATUS:\n- DEADLINE:\n"
# Expected contents of the standard note template.
# Ожидаемое содержимое стандартного шаблона заметки.
NOTE = "- NAME:\n- TAGS:\n"
# Expected field order in the full JSON output.
# Ожидаемый порядок полей в полном выводе JSON.
FIELDS = ("path", "time", "mtime", "name", "priority", "deadline", "status", "tags")


# Runs the real CLI in an isolated directory per test.
# Запускает настоящий CLI в отдельном каталоге для каждого теста.
class Cli_Tests(unittest.TestCase):
    # Creates the test fixture and registers cleanup. Filesystem errors fail setup.
    # Создаёт тестовое окружение и регистрирует очистку. Сбой файловой системы прерывает
    # подготовку.
    def setUp(self):
        fixture = tempfile.TemporaryDirectory(prefix=".mado-test-cli-", dir=PROJECT)
        self.addCleanup(fixture.cleanup)
        self.work = Path(fixture.name) / "work with spaces"
        self.work.mkdir()
        self.repo = self.work / "MADO"

    # Captures process output and status. Launch errors and timeouts propagate.
    # Перехватывает вывод и код выхода процесса. Ошибки запуска и превышение времени
    # передаются вызывающему коду.
    def run_mado(self, *args, cwd=None):
        return subprocess.run(
            [str(MADO), *args],
            cwd=self.work if cwd is None else cwd,
            capture_output=True,
            text=True,
            encoding=locale.getpreferredencoding(False),
            timeout=10,
        )

    # Checks all supplied conditions. Raises AssertionError on failure.
    # Проверяет все условия. При провале выбрасывает AssertionError.
    def test(self, condition, *checks):
        self.assertTrue(condition and all(checks))

    # Checks success, exact stdout, and empty stderr. Returns false on mismatch.
    # Проверяет успех, точный stdout и пустой stderr. При несовпадении возвращает false.
    def check_output(self, result, expected):
        return (result.returncode, result.stdout, result.stderr) == (0, expected, "")

    # Checks a rejected command and its diagnostic. Returns false on mismatch.
    # Проверяет отказ команды и диагностику. При несовпадении возвращает false.
    def check_error(self, result, message):
        return result.returncode == 1 and result.stdout == "" and message in result.stderr

    # Writes a fixture entry. Filesystem errors propagate.
    # Записывает тестовую запись. Ошибки файловой системы передаются вызывающему коду.
    def entry(self, timestamp, content):
        path = self.repo / timestamp / "MAIN.md"
        path.parent.mkdir(parents=True)
        path.write_text(content, encoding="utf-8")
        return path

    # Checks that legacy names resolve to canonical help without being listed.
    # Проверяет, что старые имена открывают основную справку и не выводятся в списке.
    def test_legacy_command_aliases(self):
        aliases = {
            "help": ("usage",),
            "ls": ("list", "show", "search", "find", "view"),
            "rm": ("remove", "delete", "clear", "trash"),
            "init": ("setup", "start", "install"),
            "new": ("add", "create", "note", "task"),
            "info": ("repo", "about", "summary", "stat"),
            "debug": ("check",),
        }
        general = self.run_mado("help")
        # Use the help alias to look up every alias without modifying a repository.
        # Ищем каждый алиас через алиас help без изменения репозитория.
        for command, names in aliases.items():
            expected = self.run_mado("help", command)
            for alias in names:
                with self.subTest(alias=alias):
                    actual = self.run_mado("usage", alias)
                    self.test(expected.returncode == 0 and
                              (actual.returncode, actual.stdout, actual.stderr) ==
                              (0, expected.stdout, expected.stderr) and
                              not re.search(rf"^  {alias}\s+-", general.stderr, re.MULTILINE))

    # Checks that invoking a legacy alias runs the command with its current flags.
    # Проверяет, что вызов старого алиаса запускает команду с текущими флагами.
    def test_legacy_list_alias_dispatch(self):
        entry = self.entry("20260101-000000", "- NAME: Example\n")
        result = self.run_mado("list", "-format", "path", "name = Example")
        self.test(self.check_output(result, str(entry) + "\n"))

    # Checks general help. Failures are reported by unittest.
    # Проверяет общую справку. Сбои регистрирует unittest.
    def test_general_help(self):
        result = self.run_mado("help")
        self.test(
            result.returncode == 0 and result.stdout == "",
            all(re.search(rf"^  {name}\s+-", result.stderr, re.MULTILINE)
                for name in ("help", "version", "ls", "rm", "init", "new", "info", "debug")),
            "<command> [OPTIONS]" in result.stderr and "-C <path>" in result.stderr,
        )

    # Checks command help. Failures are reported by unittest.
    # Проверяет справку по командам. Сбои регистрирует unittest.
    def test_command_help(self):
        signatures = {
            "help": "[COMMAND]", "version": "version", "ls": "[QUERY]",
            "rm": "<QUERY>", "init": "[-force]", "new": "[-template <name>]",
            "info": "[-only-dir]", "debug": "<QUERY>",
        }
        for command, signature in signatures.items():
            with self.subTest(command=command):
                result = self.run_mado("help", command)
                self.test(result.returncode == 0 and result.stdout == "" and
                          result.stderr.startswith("Usage: ") and signature in result.stderr)

    # Checks unknown help topic. Failures are reported by unittest.
    # Проверяет ошибку справки для неизвестной команды. Сбои регистрирует unittest.
    def test_unknown_help_topic(self):
        self.test(self.check_error(self.run_mado("help", "missing"), "Unknown command: missing"))

    # Checks version. Failures are reported by unittest.
    # Проверяет вывод версии. Сбои регистрирует unittest.
    def test_version(self):
        result = self.run_mado("version")
        self.test(result.returncode == 0 and result.stderr == "" and
                  re.fullmatch(r"mado version \d+\.\d+\.\d+\n", result.stdout) is not None)

    # Checks global argument errors. Failures are reported by unittest.
    # Проверяет ошибки глобальных аргументов. Сбои регистрирует unittest.
    def test_global_argument_errors(self):
        cases = [
            ((), "No command is provided"),
            (("missing",), "Unknown command: missing"),
            (("-missing",), "unknown flag"),
            (("-C",), "no value provided"),
            (("-C", "missing", "info"), "Cannot change to missing"),
        ]
        for args, message in cases:
            with self.subTest(args=args):
                self.test(self.check_error(self.run_mado(*args), message))

    # Checks global working directory. Failures are reported by unittest.
    # Проверяет смену рабочего каталога. Сбои регистрирует unittest.
    def test_global_working_directory(self):
        self.repo.mkdir()
        nested = self.work / "nested dir"
        nested.mkdir()
        self.test(self.check_output(
            self.run_mado("-C", str(nested), "info", "-only-dir", cwd=self.work.parent),
            str(self.repo) + "\n",
        ))

    # Checks command argument errors. Failures are reported by unittest.
    # Проверяет ошибки аргументов команд. Сбои регистрирует unittest.
    def test_command_argument_errors(self):
        cases = [
            (("ls", "all", "all"), "QUERY must be a single argument"),
            (("rm",), "QUERY is required"),
            (("rm", "all", "all"), "QUERY must be a single argument"),
            (("init", "extra"), "init takes no positional arguments"),
            (("new", "extra"), "new takes no positional arguments"),
            (("info", "extra"), "Usage:"),
            (("ls", "-format"), "Usage:"),
            (("new", "-template"), "Usage:"),
            (("init", "-unknown"), "Usage:"),
        ]
        for args, message in cases:
            with self.subTest(args=args):
                self.test(self.check_error(self.run_mado(*args), message))

    # Checks invalid formats. Failures are reported by unittest.
    # Проверяет отклонение неверных форматов. Сбои регистрирует unittest.
    def test_invalid_formats(self):
        for args in (("ls",), ("rm",), ("new",), ("info",)):
            for fmt in ("", "zzzz"):
                with self.subTest(command=args[0], format=fmt):
                    query = ("all",) if args[0] == "rm" else ()
                    message = "Invalid format for info:" if args[0] == "info" else "Unknown format:"
                    self.test(self.check_error(self.run_mado(*args, "-format", fmt, *query), message))

    # Checks commands without repository. Failures are reported by unittest.
    # Проверяет команды вне репозитория. Сбои регистрирует unittest.
    def test_commands_without_repository(self):
        # Project-local fixtures have a real MADO ancestor in this checkout.
        # Над тестовыми каталогами внутри проекта находится настоящий MADO.
        root = Path(self.work.anchor)
        if (root / "MADO").is_dir():
            self.skipTest("filesystem root contains a MADO repository")
        for args in (("ls",), ("rm", "all"), ("new",), ("info",)):
            with self.subTest(args=args):
                self.test(self.check_error(self.run_mado(*args, cwd=root), "No MADO/ directory found"))

    # Checks init creates repository. Failures are reported by unittest.
    # Проверяет создание репозитория. Сбои регистрирует unittest.
    def test_init_creates_repository(self):
        result = self.run_mado("init", "-force")
        templates = self.repo / "templates"
        self.test(self.check_output(result, str(self.repo) + "\n") and
                  (templates / "note.md").read_text() == NOTE and
                  (templates / "task.md").read_text() == TASK)

    # Checks init restores ancestor templates. Failures are reported by unittest.
    # Проверяет восстановление родительских шаблонов. Сбои регистрирует unittest.
    def test_init_restores_ancestor_templates(self):
        templates = self.repo / ".templates"
        templates.mkdir(parents=True)
        (templates / "note.md").write_text("changed", encoding="utf-8")
        (templates / "custom.md").write_text("custom", encoding="utf-8")
        nested = self.work / "nested"
        nested.mkdir()
        result = self.run_mado("init", cwd=nested)
        self.test(self.check_output(result, str(self.repo) + "\n") and
                  (templates / "note.md").read_text() == NOTE and
                  (templates / "task.md").read_text() == TASK and
                  (templates / "custom.md").read_text() == "custom" and
                  not (nested / "MADO").exists())

    # Checks init force creates nested repository. Failures are reported by unittest.
    # Проверяет принудительное создание вложенного репозитория. Сбои регистрирует unittest.
    def test_init_force_creates_nested_repository(self):
        self.repo.mkdir()
        nested = self.work / "nested"
        nested.mkdir()
        result = self.run_mado("init", "-force", cwd=nested)
        self.test(self.check_output(result, str(nested / "MADO") + "\n") and
                  (nested / "MADO" / "templates" / "note.md").is_file() and
                  not (self.repo / "templates").exists())

    # Checks init rejects mado file. Failures are reported by unittest.
    # Проверяет отказ init при наличии файла MADO. Сбои регистрирует unittest.
    def test_init_rejects_mado_file(self):
        self.repo.write_text("keep me", encoding="utf-8")
        result = self.run_mado("init")
        self.test(self.check_error(result, "MADO") and self.repo.read_text() == "keep me")

    # Checks init reports template write error. Failures are reported by unittest.
    # Проверяет диагностику ошибки записи шаблона. Сбои регистрирует unittest.
    def test_init_reports_template_write_error(self):
        (self.repo / "templates" / "note.md").mkdir(parents=True)
        self.test(self.check_error(self.run_mado("init"), "Cannot write template"))

    # Checks new uses builtin without templates. Failures are reported by unittest.
    # Проверяет использование встроенного шаблона. Сбои регистрирует unittest.
    def test_new_uses_builtin_without_templates(self):
        self.repo.mkdir()
        result = self.run_mado("new", "-suffix", "-example", "-format", "pth")
        path = Path(result.stdout.strip())
        self.test(self.check_output(result, str(path) + "\n") and path.is_file() and
                  path.parent.parent == self.repo and path.name == "MAIN.md" and
                  re.fullmatch(r"\d{8}-\d{6}-example", path.parent.name) is not None and
                  path.read_text() == TASK)

    # Checks new copies named template. Failures are reported by unittest.
    # Проверяет копирование именованного шаблона. Сбои регистрирует unittest.
    def test_new_copies_named_template(self):
        templates = self.repo / "_templates"
        templates.mkdir(parents=True)
        content = b'- NAME: a "quoted" name\n- TAGS: bug\n\nBody\n'
        (templates / "custom.md").write_bytes(content)
        result = self.run_mado("new", "-template", "custom", "-format", "js")
        row = json.loads(result.stdout)
        self.test(result.returncode == 0 and result.stderr == "" and
                  row["name"] == 'a "quoted" name' and row["tags"] == ["bug"] and
                  Path(row["path"]).read_bytes() == content)

    # Checks new missing templates directory. Failures are reported by unittest.
    # Проверяет ошибку при отсутствии каталога шаблонов. Сбои регистрирует unittest.
    def test_new_missing_templates_directory(self):
        self.repo.mkdir()
        self.test(self.check_error(self.run_mado("new", "-template", "note"), "mado init") and
                  not list(self.repo.iterdir()))

    # Checks new rejects unsafe inputs. Failures are reported by unittest.
    # Проверяет отклонение небезопасных аргументов new. Сбои регистрирует unittest.
    def test_new_rejects_unsafe_inputs(self):
        self.repo.mkdir()
        cases = [
            ("-suffix", "../escape", "Invalid suffix"),
            ("-suffix", "a:b", "Invalid suffix"),
            ("-suffix", "a\\b", "Invalid suffix"),
            ("-template", "../note", "Invalid template name"),
            ("-template", "a:b", "Invalid template name"),
            ("-template", "", "Invalid template name"),
        ]
        for flag, value, message in cases:
            with self.subTest(flag=flag, value=value):
                self.test(self.check_error(self.run_mado("new", flag, value), message) and
                          not list(self.repo.iterdir()))

    # Checks ls filters sorts and formats paths. Failures are reported by unittest.
    # Проверяет фильтрацию, сортировку и вывод путей в ls. Сбои регистрирует unittest.
    def test_ls_filters_sorts_and_formats_paths(self):
        older = self.entry("20260101-000000", "- NAME: Fix one\n- PRIORITY: 1\n")
        newer = self.entry("20260102-000000", "- NAME: Fix two\n- PRIORITY: 1\n")
        self.entry("20260103-000000", "- NAME: Ignore\n- PRIORITY: 0\n")
        result = self.run_mado("ls", "-format", "pth", "-sort", " + prio , - time ",
                               "priority = 1")
        self.test(self.check_output(result, f"{newer}\n{older}\n"))

    # Checks ls default output and hidden path. Failures are reported by unittest.
    # Проверяет обычный вывод ls и скрытие поля пути. Сбои регистрирует unittest.
    def test_ls_default_output_and_hidden_path(self):
        path = self.entry("20260101-000000", "- NAME: Example\n")
        result = self.run_mado("ls", "-hide-path", "-only-hidden", "-hide-name")
        hidden_path = self.run_mado("ls", "-only-hidden", "-hide-name")
        self.test(self.check_output(result, f"{path}:1: PATH:[{path}] NAME:[Example]\n") and
                  self.check_output(hidden_path, f"{path}:1: NAME:[Example]\n"))

    # Checks ls json visibility. Failures are reported by unittest.
    # Проверяет видимость полей JSON в ls. Сбои регистрирует unittest.
    def test_ls_json_visibility(self):
        path = self.entry("20260101-000000", "- NAME: Example\n- TAGS: bug\n")
        result = self.run_mado("ls", "-format", "js")
        row = json.loads(result.stdout)
        hidden = [
            json.loads(self.run_mado("ls", "-format", "jsonl", f"-hide-{field}").stdout)
            for field in FIELDS
        ]
        only = self.run_mado("ls", "-format", "js", "-only-hidden", "-hide-name", "-hide-tags")
        self.test(result.returncode == 0 and result.stderr == "" and tuple(row) == FIELDS and
                  row["path"] == str(path) and row["name"] == "Example" and
                  all(actual == {key: value for key, value in row.items() if key != field}
                      for field, actual in zip(FIELDS, hidden)) and
                  self.check_output(only, '{"name":"Example","tags":["bug"]}\n') and
                  self.check_output(self.run_mado("ls", "-format", "jsonl", "-only-hidden"), "{}\n"))

    # Checks ls invalid sort. Failures are reported by unittest.
    # Проверяет отклонение неверного критерия сортировки. Сбои регистрирует unittest.
    def test_ls_invalid_sort(self):
        for spec in ("", "+", "zzzz"):
            with self.subTest(spec=spec):
                self.test(self.check_error(self.run_mado("ls", "-sort", spec), "Invalid sort specification"))

    # Checks invalid queries do not mutate. Failures are reported by unittest.
    # Проверяет сохранность записей при неверных запросах. Сбои регистрирует unittest.
    def test_invalid_queries_do_not_mutate(self):
        entry = self.entry("20260101-000000", "- NAME: Keep\n")
        for command in ("ls", "rm"):
            with self.subTest(command=command):
                result = self.run_mado(command, "name =")
                self.test(self.check_error(result, "Expected string") and entry.is_file())

    # Checks rm removes only matching entries. Failures are reported by unittest.
    # Проверяет удаление только подходящих записей. Сбои регистрирует unittest.
    def test_rm_removes_only_matching_entries(self):
        removed = self.entry("20260101-000000", "- NAME: Remove\n")
        kept = self.entry("20260102-000000", "- NAME: Keep\n")
        result = self.run_mado("rm", "-format", "path", "name = remove")
        self.test(self.check_output(result, str(removed) + "\n") and
                  not removed.parent.exists() and kept.is_file())

    # Checks rm no matches. Failures are reported by unittest.
    # Проверяет поведение rm при отсутствии совпадений. Сбои регистрирует unittest.
    def test_rm_no_matches(self):
        kept = self.entry("20260101-000000", "- NAME: Keep\n")
        self.test(self.check_output(self.run_mado("rm", "name = missing"), "") and kept.is_file())

    # Checks info reports counts. Failures are reported by unittest.
    # Проверяет счётчики info. Сбои регистрирует unittest.
    def test_info_reports_counts(self):
        self.entry("20260101-000000", "- STATUS: open\n- TAGS: bug, docs\n- PRIORITY: 7\n")
        self.entry("20260102-000000", "- STATUS: closed\n- TAGS: bug\n- PRIORITY: 7\n")
        result = self.run_mado("info", "-format", "js")
        self.test(result.returncode == 0 and result.stderr == "" and json.loads(result.stdout) == {
            "main_dir": str(self.repo), "entries_count": 2, "statuses_count": 2,
            "tags_count": 2, "priorities_count": 1, "statuses": {"closed": 1, "open": 1},
            "tags": {"bug": 2, "docs": 1}, "priorities": {"7": 2},
        } and self.check_output(self.run_mado("info"),
                                f"Main directory: {self.repo}\nEntries count: 2\n"
                                "Statuses (2):\n  closed: 1\n  open: 1\n"
                                "Tags (2):\n  bug: 2\n  docs: 1\nPriorities (1):\n  7: 2\n"))

    # Checks info empty repository. Failures are reported by unittest.
    # Проверяет info в пустом репозитории. Сбои регистрирует unittest.
    def test_info_empty_repository(self):
        self.repo.mkdir()
        self.test(self.check_output(self.run_mado("info"),
                                    f"Main directory: {self.repo}\nEntries count: 0\n"
                                    "Statuses (0):\nTags (0):\nPriorities (0):\n"))

    # Checks info directory formats. Failures are reported by unittest.
    # Проверяет форматы вывода каталога в info. Сбои регистрирует unittest.
    def test_info_directory_formats(self):
        self.repo.mkdir()
        result = self.run_mado("info", "-format", "js", "-only-dir")
        self.test(result.returncode == 0 and result.stderr == "" and
                  json.loads(result.stdout) == str(self.repo) and
                  self.check_output(self.run_mado("info", "-only-dir"), str(self.repo) + "\n"))

    # Checks info rejects path format. Failures are reported by unittest.
    # Проверяет отклонение формата path в info. Сбои регистрирует unittest.
    def test_info_rejects_path_format(self):
        self.test(self.check_error(self.run_mado("info", "-format", "path"), "Invalid format for info"))


    # Checks AST output from a compound query. unittest reports failures.
    # Проверяет вывод AST составного запроса. Сбои регистрирует unittest.
    def test_debug_prints_ast(self):
        result = self.run_mado("debug", "priority = 3 and not all")
        self.test(self.check_output(result,
                                    "Binary: And\n  Comparison: Priority Eq 3\n"
                                    "  Unary: Not\n    All\n"))

    # Checks that an invalid query produces a diagnostic and no AST.
    # Проверяет диагностику неверного запроса и отсутствие AST в выводе.
    def test_debug_invalid_query(self):
        self.test(self.check_error(self.run_mado("debug", "name ="), "Expected string"))

    # Checks that an empty query is rejected. unittest reports failures.
    # Проверяет отказ для пустого запроса. Сбои регистрирует unittest.
    def test_debug_empty_query(self):
        self.test(self.check_error(self.run_mado("debug", ""), "Empty query"))

    # Checks rejection of missing and extra arguments. unittest reports failures.
    # Проверяет отказ при отсутствии аргумента или лишних аргументах.
    def test_debug_argument_errors(self):
        self.test(self.check_error(self.run_mado("debug"), "QUERY is required") and
                  self.check_error(self.run_mado("debug", "all", "all"),
                                   "QUERY must be a single argument") and
                  self.check_error(self.run_mado("debug", "-unknown"), "Usage:"))


# Runs the suite and exits with unittest's success or failure status.
# Запускает набор и завершает процесс с кодом результата unittest.
def main():
    loader = unittest.TestLoader()
    loader.testMethodPrefix = "test_"
    unittest.main(testLoader=loader)


if __name__ == "__main__":
    main()
