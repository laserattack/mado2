import json
import locale
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


MADO = Path(sys.argv.pop(1)).resolve()
PROJECT = Path(sys.argv.pop(1)).resolve()
TASK = "- NAME:\n- PRIORITY:\n- TAGS:\n- STATUS:\n- DEADLINE:\n"
NOTE = "- NAME:\n- TAGS:\n"
FIELDS = ("path", "time", "mtime", "name", "priority", "deadline", "status", "tags")


class Cli_Tests(unittest.TestCase):
    def setUp(self):
        fixture = tempfile.TemporaryDirectory(prefix=".mado-test-cli-", dir=PROJECT)
        self.addCleanup(fixture.cleanup)
        self.work = Path(fixture.name) / "work with spaces"
        self.work.mkdir()
        self.repo = self.work / "MADO"

    def run_mado(self, *args, cwd=None):
        return subprocess.run(
            [str(MADO), *args],
            cwd=self.work if cwd is None else cwd,
            capture_output=True,
            text=True,
            encoding=locale.getpreferredencoding(False),
            timeout=10,
        )

    def test(self, condition, *checks):
        self.assertTrue(condition and all(checks))

    def check_output(self, result, expected):
        return (result.returncode, result.stdout, result.stderr) == (0, expected, "")

    def check_error(self, result, message):
        return result.returncode == 1 and result.stdout == "" and message in result.stderr

    def entry(self, timestamp, content):
        path = self.repo / timestamp / "MAIN.md"
        path.parent.mkdir(parents=True)
        path.write_text(content, encoding="utf-8")
        return path

    def test_legacy_command_aliases(self):
        aliases = {
            "help": ("usage",),
            "ls": ("list", "show", "search", "find", "view"),
            "rm": ("remove", "delete", "clear", "trash"),
            "init": ("setup", "start", "install"),
            "new": ("add", "create", "note", "task"),
            "info": ("repo", "about", "summary", "stat"),
            "debug-ast": ("debug", "check"),
        }
        general = self.run_mado("help")
        for command, names in aliases.items():
            expected = self.run_mado("help", command)
            for alias in names:
                with self.subTest(alias=alias):
                    actual = self.run_mado("usage", alias)
                    self.test(expected.returncode == 0 and
                              (actual.returncode, actual.stdout, actual.stderr) ==
                              (0, expected.stdout, expected.stderr) and
                              not re.search(rf"^  {alias}\s+-", general.stderr, re.MULTILINE))

    def test_legacy_list_alias_dispatch(self):
        entry = self.entry("20260101-000000", "- NAME: Example\n")
        result = self.run_mado("list", "-format", "path", "name = Example")
        self.test(self.check_output(result, str(entry) + "\n"))

    def test_legacy_list_flags(self):
        self.entry("20260101-000000", "- NAME: Older\n")
        self.entry("20260102-000000", "- NAME: Newer\n")
        expected = self.run_mado("ls", "-sort", "-time", "-format", "js",
                                 "-only-hidden", "-hide-name")
        self.test(self.check_output(expected, '{"name":"Newer"}\n{"name":"Older"}\n') and
                  self.check_output(self.run_mado("list", "-s", "-time", "-f", "js", "-O", "-N"),
                                    expected.stdout) and
                  self.check_output(self.run_mado("list", "--sort=-time", "-format", "path",
                                                  "--format=js", "--only-hidden", "--hide-name"),
                                    expected.stdout))

    def test_legacy_hide_flags(self):
        self.entry("20260101-000000", "- NAME: Example\n")
        for field, short in zip(FIELDS, "HTMNPDSG"):
            with self.subTest(field=field):
                expected = self.run_mado("ls", "-format", "jsonl", "-only-hidden", f"-hide-{field}")
                self.test(expected.returncode == 0 and expected.stderr == "" and
                          list(json.loads(expected.stdout)) == [field] and
                          self.check_output(self.run_mado("ls", "-f", "jsonl", "-O", f"-{short}"),
                                            expected.stdout) and
                          self.check_output(self.run_mado("ls", "--format", "jsonl", "--only-hidden",
                                                          f"--hide-{field}"), expected.stdout))

    def test_legacy_info_flags(self):
        self.repo.mkdir()
        expected = json.dumps(str(self.repo), ensure_ascii=False) + "\n"
        self.test(self.check_output(self.run_mado("--working-dir", str(self.work),
                                                  "stat", "-f", "js", "-d",
                                                  cwd=self.work.parent), expected) and
                  self.check_output(self.run_mado("info", "--format=jsonl", "--dir"), expected))

    def test_legacy_init_and_new_flags(self):
        for index, (force, template, fmt) in enumerate(
                (("-F", "-t", "-f"), ("--force", "--template", "--format"))):
            with self.subTest(force=force, template=template):
                work = self.work / str(index)
                work.mkdir()
                initialized = self.run_mado("setup", force, cwd=work)
                created = self.run_mado("add", template, "note", fmt, "path", cwd=work)
                path = Path(created.stdout.strip())
                self.test(self.check_output(initialized, str(work / "MADO") + "\n") and
                          self.check_output(created, str(path) + "\n") and
                          path.is_file() and path.read_text() == NOTE)

    def test_mtimes_commands(self):
        self.run_mado("init", "-force")
        main = self.entry("20260101-000000", "- NAME: sample\n")
        original = main.stat().st_mtime_ns
        dumped = self.run_mado("dump-mtimes")
        index = self.repo / "mtimes"
        custom = self.work / "custom times"
        copied = self.run_mado("dump-mtimes", str(custom))
        os.utime(main, ns=(original, original + 10000000000))
        restored = self.run_mado("restore-mtimes")
        default_time = main.stat().st_mtime_ns
        os.utime(main, ns=(original, original + 20000000000))
        explicit = self.run_mado("restore-mtimes", str(custom))
        self.test(self.check_output(dumped, str(index) + "\n") and self.check_output(copied, str(custom) + "\n") and
                  self.check_output(restored, str(index) + "\n") and self.check_output(explicit, str(custom) + "\n") and
                  index.read_bytes() == custom.read_bytes() and default_time == original and
                  main.stat().st_mtime_ns == original)

    def test_mtimes_strict_flag(self):
        self.run_mado("init", "-force")
        main = self.entry("20260101-000000", "- NAME: existing\n")
        missing = self.entry("20260102-000000", "- NAME: missing\n")
        original = main.stat().st_mtime_ns
        custom = self.work / "custom times"
        self.run_mado("dump-mtimes", str(custom))
        missing.unlink()
        os.utime(main, ns=(original, original + 10000000000))
        changed = main.stat().st_mtime_ns
        strict = self.run_mado("restore-mtimes", "-strict", str(custom))
        unchanged = main.stat().st_mtime_ns == changed
        restored = self.run_mado("restore-mtimes", str(custom))
        self.test(self.check_error(strict, "Missing regular entry file") and unchanged and
                  self.check_output(restored, str(custom) + "\n") and main.stat().st_mtime_ns == original)

    def test_mtimes_keep_newer_flag(self):
        self.run_mado("init", "-force")
        main = self.entry("20260101-000000", "- NAME: newer\n")
        missing = self.entry("20260102-000000", "- NAME: missing\n")
        original = main.stat().st_mtime_ns
        custom = self.work / "custom times"
        self.run_mado("dump-mtimes", str(custom))

        os.utime(main, ns=(original, original + 10000000000))
        newer = main.stat().st_mtime_ns
        kept = self.run_mado("restore-mtimes", "-strict", "-keep-newer", str(custom))
        preserved = main.stat().st_mtime_ns == newer
        missing.unlink()
        strict = self.run_mado("restore-mtimes", "-strict", "-keep-newer", str(custom))
        unchanged = main.stat().st_mtime_ns == newer
        skipped = self.run_mado("restore-mtimes", "-keep-newer", str(custom))
        still_newer = main.stat().st_mtime_ns == newer
        overwritten = self.run_mado("restore-mtimes", str(custom))

        self.test(self.check_output(kept, str(custom) + "\n") and preserved and
                  self.check_error(strict, "Missing regular entry file") and unchanged and
                  self.check_output(skipped, str(custom) + "\n") and still_newer and
                  self.check_output(overwritten, str(custom) + "\n") and main.stat().st_mtime_ns == original)

    def test_mtimes_index_discovery(self):
        self.run_mado("init", "-force")
        main = self.entry("20260101-000000", "- NAME: entry\n")
        original = main.stat().st_mtime_ns
        dumped = self.run_mado("dump-mtimes")
        default = self.repo / "mtimes"
        legacy = self.repo / "_mtimes"
        default.rename(legacy)

        os.utime(main, ns=(original, original + 10000000000))
        restored = self.run_mado("restore-mtimes")
        help_result = self.run_mado("help")

        self.test(self.check_output(dumped, str(default) + "\n") and self.check_output(restored, str(legacy) + "\n") and
                  main.stat().st_mtime_ns == original and not default.exists() and legacy.is_file() and
                  "Save entry modification times to MADO/mtimes or FILE" in help_result.stderr and
                  "Restore entry modification times from MADO/mtimes or FILE" in help_result.stderr and
                  all(f"MADO/{name}" not in help_result.stderr for name in (".mtimes", "_mtimes")))

    def test_mtimes_relative_output_path(self):
        self.run_mado("init", "-force")
        dumped = self.run_mado("dump-mtimes", "backup times")
        restored = self.run_mado("restore-mtimes", "backup times")
        expected = str(self.work / "backup times") + "\n"

        self.test(self.check_output(dumped, expected) and self.check_output(restored, expected))

    def test_mtimes_dump_failure_output(self):
        self.run_mado("init", "-force")
        result = self.run_mado("dump-mtimes", "missing/index")

        self.test(self.check_error(result, "Cannot write mtime index"))

    def test_mtimes_command_errors(self):
        root = Path(self.work.anchor)
        if (root / "MADO").is_dir():
            self.skipTest("filesystem root contains a MADO repository")
        missing_repo = [self.run_mado(command, cwd=root)
                        for command in ("dump-mtimes", "restore-mtimes")]
        self.run_mado("init", "-force")
        missing_index = self.run_mado("restore-mtimes")
        extra = [self.run_mado(command, "one", "two")
                 for command in ("dump-mtimes", "restore-mtimes")]
        flags = [self.run_mado(command, "-unknown")
                 for command in ("dump-mtimes", "restore-mtimes")]
        self.test(all(self.check_error(result, "No MADO/") for result in missing_repo) and
                  self.check_error(missing_index, "Cannot read mtime index") and
                  all(self.check_error(result, "Usage:") for result in extra + flags))

    def test_general_help(self):
        result = self.run_mado("help")
        self.test(
            result.returncode == 0 and result.stdout == "",
            all(re.search(rf"^  {name}\s+-", result.stderr, re.MULTILINE)
                for name in ("help", "version", "ls", "rm", "init", "new", "info", "debug-ast")),
            "<command> [OPTIONS]" in result.stderr and "-C <path>" in result.stderr,
        )

    def test_command_help(self):
        signatures = {
            "help": "[COMMAND]", "version": "version", "ls": "[QUERY]",
            "rm": "<QUERY>", "init": "[-force]", "new": "[-template <name>]",
            "info": "[-only-dir]", "debug-ast": "<QUERY>",
            "dump-mtimes": "[FILE]", "restore-mtimes": "[-strict] [-keep-newer] [FILE]",
        }
        for command, signature in signatures.items():
            with self.subTest(command=command):
                result = self.run_mado("help", command)
                self.test(result.returncode == 0 and result.stdout == "" and
                          result.stderr.startswith("Usage: ") and signature in result.stderr)

    def test_unknown_help_topic(self):
        self.test(self.check_error(self.run_mado("help", "missing"), "Unknown command: missing"))

    def test_version(self):
        result = self.run_mado("version")
        self.test(result.returncode == 0 and result.stderr == "" and
                  re.fullmatch(r"mado version \d+\.\d+\.\d+\n", result.stdout) is not None)

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

    def test_global_working_directory(self):
        self.repo.mkdir()
        nested = self.work / "nested dir"
        nested.mkdir()
        self.test(self.check_output(
            self.run_mado("-C", str(nested), "info", "-only-dir", cwd=self.work.parent),
            str(self.repo) + "\n",
        ))

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

    def test_invalid_formats(self):
        for args in (("ls",), ("rm",), ("new",), ("info",)):
            for fmt in ("", "zzzz"):
                with self.subTest(command=args[0], format=fmt):
                    query = ("all",) if args[0] == "rm" else ()
                    message = "Invalid format for info:" if args[0] == "info" else "Unknown format:"
                    self.test(self.check_error(self.run_mado(*args, "-format", fmt, *query), message))

    def test_commands_without_repository(self):
        # Project-local fixtures have a real MADO ancestor in this checkout.
        root = Path(self.work.anchor)
        if (root / "MADO").is_dir():
            self.skipTest("filesystem root contains a MADO repository")
        for args in (("ls",), ("rm", "all"), ("new",), ("info",)):
            with self.subTest(args=args):
                self.test(self.check_error(self.run_mado(*args, cwd=root), "No MADO/ directory found"))

    def test_init_creates_repository(self):
        result = self.run_mado("init", "-force")
        templates = self.repo / "templates"
        self.test(self.check_output(result, str(self.repo) + "\n") and
                  (templates / "note.md").read_text() == NOTE and
                  (templates / "task.md").read_text() == TASK)

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

    def test_init_force_creates_nested_repository(self):
        self.repo.mkdir()
        nested = self.work / "nested"
        nested.mkdir()
        result = self.run_mado("init", "-force", cwd=nested)
        self.test(self.check_output(result, str(nested / "MADO") + "\n") and
                  (nested / "MADO" / "templates" / "note.md").is_file() and
                  not (self.repo / "templates").exists())

    def test_init_rejects_mado_file(self):
        self.repo.write_text("keep me", encoding="utf-8")
        result = self.run_mado("init")
        self.test(self.check_error(result, "MADO") and self.repo.read_text() == "keep me")

    def test_init_reports_template_write_error(self):
        (self.repo / "templates" / "note.md").mkdir(parents=True)
        self.test(self.check_error(self.run_mado("init"), "Cannot write template"))

    def test_new_uses_builtin_without_templates(self):
        self.repo.mkdir()
        result = self.run_mado("new", "-suffix", "-example", "-format", "pth")
        path = Path(result.stdout.strip())
        self.test(self.check_output(result, str(path) + "\n") and path.is_file() and
                  path.parent.parent == self.repo and path.name == "MAIN.md" and
                  re.fullmatch(r"\d{8}-\d{6}-example", path.parent.name) is not None and
                  path.read_text() == TASK)

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

    def test_new_missing_templates_directory(self):
        self.repo.mkdir()
        self.test(self.check_error(self.run_mado("new", "-template", "note"), "mado init") and
                  not list(self.repo.iterdir()))

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

    def test_ls_filters_sorts_and_formats_paths(self):
        older = self.entry("20260101-000000", "- NAME: Fix one\n- PRIORITY: 1\n")
        newer = self.entry("20260102-000000", "- NAME: Fix two\n- PRIORITY: 1\n")
        self.entry("20260103-000000", "- NAME: Ignore\n- PRIORITY: 0\n")
        result = self.run_mado("ls", "-format", "pth", "-sort", " + prio , - time ",
                               "priority = 1")
        self.test(self.check_output(result, f"{newer}\n{older}\n"))

    def test_ls_default_output_and_hidden_path(self):
        path = self.entry("20260101-000000", "- NAME: Example\n")
        result = self.run_mado("ls", "-hide-path", "-only-hidden", "-hide-name")
        hidden_path = self.run_mado("ls", "-only-hidden", "-hide-name")
        self.test(self.check_output(result, f"{path}:1: PATH:[{path}] NAME:[Example]\n") and
                  self.check_output(hidden_path, f"{path}:1: NAME:[Example]\n"))

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

    def test_ls_invalid_sort(self):
        for spec in ("", "+", "zzzz"):
            with self.subTest(spec=spec):
                self.test(self.check_error(self.run_mado("ls", "-sort", spec), "Invalid sort specification"))

    def test_invalid_queries_do_not_mutate(self):
        entry = self.entry("20260101-000000", "- NAME: Keep\n")
        for command in ("ls", "rm"):
            with self.subTest(command=command):
                result = self.run_mado(command, "name =")
                self.test(self.check_error(result, "Expected a string") and entry.is_file())

    def test_rm_removes_only_matching_entries(self):
        removed = self.entry("20260101-000000", "- NAME: Remove\n")
        kept = self.entry("20260102-000000", "- NAME: Keep\n")
        result = self.run_mado("rm", "-format", "path", "name = remove")
        self.test(self.check_output(result, str(removed) + "\n") and
                  not removed.parent.exists() and kept.is_file())

    def check_rm_failure(self, mixed):
        blocked = self.entry("20260101-000000", "- NAME: blocked\n")
        removable = self.entry("20260102-000000", "- NAME: removable\n") if mixed else None
        target = blocked if os.name == "nt" else blocked.parent
        original_permissions = target.stat().st_mode

        self.addCleanup(target.chmod, original_permissions)
        target.chmod(original_permissions & ~0o222)
        if os.name != "nt" and os.access(target, os.W_OK):
            self.skipTest("current user bypasses deletion permissions")

        result = self.run_mado("rm", "-format", "jsonl", "all")
        output = [json.loads(line) for line in result.stdout.splitlines()]
        expected_paths = [str(removable)] if mixed else []

        return (result.returncode == 1 and
                [entry["path"] for entry in output] == expected_paths and
                f"Cannot remove {blocked.parent}: " in result.stderr and
                bool(result.stderr.split(": ", 1)[-1].strip()) and blocked.is_file() and
                (not mixed or not removable.parent.exists()))

    def test_rm_partial_failure(self):
        self.test(self.check_rm_failure(True))

    def test_rm_all_failed(self):
        self.test(self.check_rm_failure(False))

    def test_rm_no_matches(self):
        kept = self.entry("20260101-000000", "- NAME: Keep\n")
        self.test(self.check_output(self.run_mado("rm", "name = missing"), "") and kept.is_file())

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

    def test_info_empty_repository(self):
        self.repo.mkdir()
        self.test(self.check_output(self.run_mado("info"),
                                    f"Main directory: {self.repo}\nEntries count: 0\n"
                                    "Statuses (0):\nTags (0):\nPriorities (0):\n"))

    def test_info_directory_formats(self):
        self.repo.mkdir()
        result = self.run_mado("info", "-format", "js", "-only-dir")
        self.test(result.returncode == 0 and result.stderr == "" and
                  json.loads(result.stdout) == str(self.repo) and
                  self.check_output(self.run_mado("info", "-only-dir"), str(self.repo) + "\n"))

    def test_info_rejects_path_format(self):
        self.test(self.check_error(self.run_mado("info", "-format", "path"), "Invalid format for info"))


    def test_debug_prints_ast(self):
        result = self.run_mado("debug-ast", "priority = 3 and not all")
        self.test(self.check_output(result,
                                    "Binary: And\n  Comparison: Priority Eq 3\n"
                                    "  Unary: Not\n    All\n"))

    def test_debug_ast_partial_output(self):
        result = self.run_mado("debug-ast", "all and")
        self.test(result.returncode == 1 and
                  result.stdout == "Binary: And\n  All\n" and
                  "query ended" in result.stderr)

    def test_debug_invalid_query(self):
        result = self.run_mado("debug-ast", "nme = )")
        self.test(result.returncode == 1 and
                  result.stdout == "Comparison: Name Eq <missing>\n" and
                  "Expected a string" in result.stderr and
                  "Field 'name' matched from 'nme'" in result.stderr)

    def test_debug_empty_query(self):
        self.test(self.check_error(self.run_mado("debug-ast", ""), "Empty query"))

    def test_debug_argument_errors(self):
        self.test(self.check_error(self.run_mado("debug-ast"), "QUERY is required") and
                  self.check_error(self.run_mado("debug-ast", "all", "all"),
                                   "QUERY must be a single argument") and
                  self.check_error(self.run_mado("debug-ast", "-unknown"), "Usage:"))


def main():
    loader = unittest.TestLoader()
    loader.testMethodPrefix = "test_"
    unittest.main(testLoader=loader)


if __name__ == "__main__":
    main()
