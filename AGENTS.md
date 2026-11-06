# Project structure

- Treat `code/libmado` as a standalone, portable library. Keep
  CLI-specific code in `code/cli` and keep the library independent of
  the CLI. Preserve portability across supported platforms.
- Put library headers under `code/libmado/include/mado/` and
  implementation files under `code/libmado/src/`, using matching
  component directories. Include library headers through their public
  paths, for example `#include <mado/common/glob.h>`.

# Platform support

- All code must support Linux, macOS, and Windows. Use portable APIs
  wherever possible.
- Guard platform-specific APIs with preprocessor conditionals
  (`#ifdef` or `#if`). Provide an implementation for each required
  platform or a portable fallback that preserves the expected behavior.

# Code documentation and naming

- Add a short comment for every function, method, class, struct,
  global variable, and similar declaration you introduce. Explain why
  it exists and summarize its behavior on success, edge cases, and
  failure. Document each returned error and exception that can
  propagate. State explicitly when there are no returned errors or
  exceptions.
- Use `Word_Word_Word` for classes and structs, `WORD_WORD` for
  global variables and constants, and `word_word` for ordinary
  variables, functions, and methods.

# Error handling

- Report OS and I/O failures with `std::runtime_error` when throwing
  an exception. Include the failed operation, the relevant path, and
  the OS error message when available. Standard exceptions such as
  `std::filesystem::filesystem_error` already derive from
  `std::runtime_error`.
- Use domain-specific exceptions such as `Repository_Error`,
  `Entry_Error`, and `Parse_Error` for invalid input, malformed data,
  and violations of application rules.

# Workspace files

- Create files and directories only inside the project root. Configure
  commands and tools to use a project-local temporary directory when
  they need one.
- Remove temporary files and directories as soon as they are no longer
  needed, including when a command fails.
- Keep all temporary files and directories out of Git. Place them under
  an ignored path or add a matching rule to `.gitignore` before creating
  them, even when automatic cleanup is in place. Never stage or commit
  temporary artifacts.

# Tests

- Each test must set up its own isolated environment, independent of
  other tests and their execution order. Clean up all test resources
  after the test, including when it fails.
- Add or update tests for every change in behavior. Cover normal use
  and relevant edge cases, such as missing required files or
  directories, invalid formats, and invalid contents.
- One test for normal use is usually enough. Add separate tests for
  distinct edge cases.
- Use exactly one `test()` call per test function. Combine multiple
  conditions in that call with `&&`.

# Git commits

- Agents must never create Git commits.
- After each complete, self-contained change, include a suggested
  commit message in the final response using the Conventional Commits
  format.
- Use `type(scope): description` when a change has a clear primary
  component. Choose a scope that names that component, such as `cli`,
  `repository`, `interpreter`, or `build`. For example,
  `feat(repository): add entry templates`.
- Use `type: description` when no single component is the primary
  focus.
