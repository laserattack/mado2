# mado

> AI-generated README. I don't speak English.

A personal CLI tool for managing tasks and notes stored as Markdown files. I made it primarily to manage entries from the Emacs compilation buffer.

For a more convenient Emacs interface, see [emado](https://github.com/laserattack/emado), which uses `transient`.

<p align="center">
  <img src="assets/mado-ghost-crt-512.png" width="512" alt="">
</p>

Each entry has its own directory containing `MAIN.md` and optional attachments. Entries can be searched, sorted, and formatted using metadata.

## Build

Requires a C compiler, a C++20 compiler, Meson, and Ninja.

```sh
meson setup build
meson compile -C build
```

The executable is at `build/code/cli/mado` on Linux/macOS, or `build\code\cli\mado.exe` on Windows (untested).

## Quick start

```sh
mado init
mado new -format path
```

Open the returned `MAIN.md` and edit it:

```markdown
- NAME: Fix login redirect
- PRIORITY: 7
- TAGS: bug, auth
- STATUS: open
- DEADLINE: 20261113-180000

The login page should redirect back to the original URL.
```

Then query your entries:

```sh
mado ls 'tag = bug and status = open'
mado ls -sort -priority,+deadline 'status = open'
mado ls -format jsonl 'priority >= 5'
mado info
```

Commands find the nearest `MADO/` directory in the current directory or its ancestors. Use `mado -C <directory>` to start elsewhere.

## Commands

| Command | Description |
| --- | --- |
| `init [-force]` | Initialize a repository and restore standard templates |
| `new [-template NAME] [-suffix TEXT]` | Create an entry |
| `ls [QUERY]` | List matching entries |
| `rm [-interactive] QUERY` | Delete matching entries and their attachments; `-interactive` asks before each deletion |
| `info` | Show repository statistics |
| `dump-mtimes [FILE]` | Save entry modification times |
| `restore-mtimes [OPTIONS] [FILE]` | Restore saved modification times |
| `debug-ast QUERY` | Print the parsed query AST |
| `help [COMMAND]` | Show help |
| `version` | Show version |

Run `mado help <command>` for details.

**Warning:** `rm` deletes entire entry directories. Preview matches with `ls` first. With `-interactive`, each entry is shown in the selected output format; only `y` or `Y` confirms deletion.

## Queries

Queries operate on metadata fields such as `name`, `tag`, `status`, `priority`, `deadline`, `time`, `mtime`, and `path`.

Supported operators:

- Comparison: `=`, `!=`, `<`, `<=`, `>`, `>=`
- String matching: `~`, `!~`, `^~`, `$~`
- Fuzzy matching: `~~`, `!~~`
- Glob matching: `%~`, `!%~`
- Logic: `not`, `and`, `xor`, `or`

Other features include `in`, `has`, `anyof`, `allof`, ranges, and predicates such as `untagged` and `undeadlined`.

```sh
mado ls 'status in (open, reopened) and tag has (bug, auth)'
mado ls 'name %~ "fix *" and priority in [3..9]'
mado ls 'name ~ anyof("login", "redirect")'
mado ls 'tag ~ allof(bug, auth)'
mado ls 'deadline >= @today and deadline < @today+7'
```

Keywords, sort fields, output formats, and time macros support fuzzy abbreviations:

```sh
mado ls -sort -prio -format js 'name ~~ "fl uncd"'
```

Available time macros include `@now`, `@today`, `@tomorrow`, `@yesterday`, `@week`, `@month`, `@year`, and `@never`. Offsets such as `@today+7` are supported.

Times and deadlines use UTC. Deadline format: `YYYYMMDD-HHMMSS` or `YYYYMMDD`.

## Output

`ls`, `new`, and `rm` support `-format default|path|jsonl`.

The default format includes `path:1:` locations for Emacs compilation mode. Use `path` for file paths or `jsonl` for scripting.

Sorting uses `-sort`, with `+` for ascending and `-` for descending:

```sh
mado ls -sort +status,-priority
```

Use `-hide-mtime`, `-hide-path`, and `-only-hidden` to control displayed fields.

## Templates

Templates live in `MADO/templates/` (also supports `.templates` and `_templates`).

```sh
mado new -template note
mado new -template bug -suffix=-login
```

Without `-template`, `new` uses the built-in task header.

`mado init` restores the standard `note.md` and `task.md` templates, overwriting edits to them. Use separate files for custom templates.

## Modification times

To preserve `MAIN.md` modification times when copying a repository:

```sh
mado dump-mtimes
# Copy MADO/ to the destination
mado restore-mtimes
```

The default index is `MADO/mtimes`. Custom paths are supported.

`restore-mtimes` also accepts `-strict` (fail if indexed entries are missing or symlinked) and `-keep-newer` (preserve newer modification times).

## License

- mado: [WTFPL](LICENSE)
- Vendored `utf8proc`: MIT and Unicode Data License
- Vendored `flag.h`: MIT
