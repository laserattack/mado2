> AI-generated README. I don't speak English.

# mado

A command-line organizer for tasks, notes, and other Markdown entries. Each
entry lives in its own directory, so it can include attachments alongside
`MAIN.md`. Use queries to filter entries by metadata, then sort and format
the results for your editor or scripts.

![A girl typing on her laptop with mado open in an evening room](assets/mado-notes-slate-540.png)

## Build

Requirements are a C compiler, a C++20 compiler, Meson, Ninja.

```sh
meson setup build
meson compile -C build
```

On Linux and macOS, the executable is `build/code/cli/mado`. If the build
succeeds on Windows, the executable should be at `build\code\cli\mado.exe`.
I haven't tested the Windows build. The examples below assume the
executable is on your `PATH`.

## Quick start

Run these commands in the directory where you want to keep your entries.

```sh
mado init
mado new -format path
```

Open the returned `MAIN.md` path and fill in the metadata. Add the body
below the header.

```markdown
- NAME: Fix login redirect
- PRIORITY: 7
- TAGS: bug, auth
- STATUS: open
- DEADLINE: 20261113-180000

The login page should redirect back to the original URL.
```

```sh
mado ls 'tag = bug and status = open'
mado ls -sort -priority,+deadline -format jsonl 'status = open'
mado info
```

Commands find the nearest `MADO/` in the current directory or an ancestor.
Use `mado -C <directory> <command>` to start from a different directory.

## Commands

| Command | What it does |
| --- | --- |
| `mado init [-force]` | Initializes the nearest repository and restores standard templates. Use `-force` to initialize here even when a repository exists above. |
| `mado new [-template NAME] [-suffix TEXT]` | Creates an entry. Use `-format path` to print its Markdown file path. |
| `mado ls [QUERY]` | Lists matching entries. An omitted query selects all entries. |
| `mado rm QUERY` | Deletes the directories of matching entries, including their attachments. |
| `mado dump-mtimes [FILE]` | Saves entry modification times; defaults to `MADO/.mtimes`. |
| `mado restore-mtimes [-strict] [FILE]` | Restores entry modification times from the index. |
| `mado info` | Shows the repository path, entry count, and counts by status, tag, and priority. |
| `mado debug-ast QUERY` | Prints the query AST, including completed nodes on a parse error. |
| `mado help [COMMAND]` | Shows general or command-specific usage. |
| `mado version` | Prints the executable version. |

Pass a query as one shell argument and put command flags before it. To
preview a deletion, run `ls` with the same query before running `rm`.

### Sorting and output

- `ls -sort +status,-priority` sorts by status ascending, then priority
  descending. Omitting a sign means ascending. Sort keys include `time`,
  `mtime`, `path`, `name`, `priority`, `deadline`, `status`, and `tags`.
- `ls`, `new`, and `rm` accept `-format default|path|jsonl`. The default
  format starts each entry with a `path:1:` location for editors. The other
  formats print paths or one JSON object per entry.
- `ls -hide-mtime -hide-path` omits those fields from entry output. The
  default format retains the location prefix. With `-only-hidden`, the
  `-hide-*` flags select the fields to include.
- `info -format jsonl` prints a single JSON object. `info -only-dir` prints
  the repository path, as a JSON string when combined with `-format jsonl`.

### Preserving modification times

`mtime` is the modification time of each entry's `MAIN.md`. Save it before
copying a repository between systems, then restore it after copying:

```sh
mado dump-mtimes
mado restore-mtimes

# Optional paths are relative to the current working directory.
mado dump-mtimes backup.mtimes
mado restore-mtimes backup.mtimes

# Require every indexed entry to exist before restoring anything.
mado restore-mtimes -strict
```

Both commands use `MADO/.mtimes` by default, finding `MADO` in the current
directory or its ancestors. Copy this index together with your entries.
The versioned text file stores the full entry directory name (including its
suffix) and Unix timestamps in nanoseconds, independent of the time zone.
The destination filesystem may have lower timestamp precision.

Restoration validates the entire index before changing times. Malformed or
duplicate records and unsafe targets are errors. Missing entry files or
directories are skipped by default. With `-strict`, a missing entry is an
error and no times are changed during validation.
Entries absent from the index remain unchanged. Filesystem write failures
stop restoration and may leave earlier entries restored; rerun after fixing
the failure. Neither command changes entry contents.

### Templates

`mado init` creates `templates/` inside `MADO/` if no supported template
directory exists. It checks `templates`, `.templates`, and `_templates`
in that order. It creates missing `note.md` and `task.md` and **restores
their standard contents if they were edited**. Put custom templates in
separate files such as `bug.md` in the selected directory.

```sh
mado new -template note -format path
mado new -template bug -suffix=-login -format path
```

Without `-template`, `new` uses the built-in task header.

## Query language

Queries match entry metadata. Use fields such as `name`, `tag`, `status`,
`priority`, `path`, `time`, `mtime`, and `deadline`. Use `any` to search
across fields and `all` to match every entry. Other standalone predicates
include `untagged`, `unstatused`, `unnamed`, `unprioritized`, and
`undeadlined`.

| Operators | Match |
| --- | --- |
| `=`, `!=` | Equality or inequality |
| `<`, `<=`, `>`, `>=` | Ordered comparison |
| `~`, `!~` | Substring or its negation |
| `^~`, `!^~` | Prefix or its negation |
| `$~`, `!$~` | Suffix or its negation |
| `~~`, `!~~` | Fuzzy match or its negation |
| `%~`, `!%~` | Glob match or its negation |

String comparisons ignore case. Quote values containing
spaces. Combine conditions using `not`, `and`, `xor`, `or`, and
parentheses.

```sh
mado ls 'name %~ "fix *" and priority in [3..9]'
mado ls 'status in (open, reopened) and tag has (bug, auth)'
mado ls 'name ~ anyof("login", "redirect")'
mado ls 'tag ~ allof(bug, auth)'
mado debug-ast 'not (status = closed or untagged)'
```

`in [low..high]` includes both bounds. Either bound can be omitted.

### Fuzzy matching

Fuzzy matching is available throughout the query and output workflow:

| Where | Example | Meaning |
| --- | --- | --- |
| Query keywords | `prio >= 5` | `priority >= 5` |
| Time macro names | `@tday` | `@today` |
| Sort fields | `-sort -prio` | Sort by priority descending |
| Output formats | `-format js` | Use JSON Lines |
| Entry values | `name ~~ "fl uncd"` | Matches a name such as “full unicode support” |

For keywords, macros, sort fields, and formats, exact names take priority
over fuzzy matches. For entry values, use `~~` to match a pattern or
`!~~` to exclude matches. The characters in the pattern must appear in
order, with gaps allowed.

```sh
mado ls -sort -prio -format js 'name ~~ "fl uncd" and deadline >= @tday'
```

### Time and dates

New entry directory names, `time`, `mtime`, and time macros use UTC.
Write deadlines in UTC using `YYYYMMDD-HHMMSS` or a shorter date such as
`YYYYMMDD`. Use `^~` to match a date prefix. In ordered comparisons, a
date-only value sorts before full timestamps on the same date.

| Macro | Value | Offset unit |
| --- | --- | --- |
| `@now` | Current UTC timestamp | Days |
| `@today`, `@yesterday`, `@tomorrow` | UTC calendar date | Days |
| `@week` | Monday of the current UTC week | Weeks |
| `@month` | Current UTC year and month | Months |
| `@year` | Current UTC year | Years |
| `@never` | `99990101-000000` | Days |

```sh
mado ls 'time >= @today and time < @tomorrow'
mado ls 'deadline >= @today and deadline < @today+7 and not undeadlined'
```

## License

- mado: [WTFPL](LICENSE)
- Vendored `utf8proc`: MIT and Unicode Data License
- Vendored `flag.h`: MIT
