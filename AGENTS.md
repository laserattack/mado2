# Project structure

- Treat `code/libmado` as a standalone, portable library. Keep
  CLI-specific code in `code/cli` and keep the library independent of
  the CLI. Preserve portability across supported platforms.
- Put library headers under `code/libmado/include/mado/` and
  implementation files under `code/libmado/src/`, using matching
  component directories. Include library headers through their public
  paths, for example `#include <mado/common/glob.h>`.

# Third-party code

- Do not modify vendored code or other files bundled with third-party
  dependencies, regardless of their location. Apply project-wide
  refactors, formatting, and documentation changes only to
  project-owned files.

# Platform support

- All code must support Linux, macOS, and Windows. Use portable APIs
  wherever possible.
- Guard platform-specific APIs with preprocessor conditionals
  (`#ifdef` or `#if`). Provide an implementation for each required
  platform or a portable fallback that preserves the expected behavior.

# Readability

- Readability is a core requirement. Prefer direct control flow and
  descriptive names. Keep comments brief and explain intent where the
  reader would otherwise have to infer it.
- Separate logical blocks inside functions and methods with blank lines.
  Keep related operations together, and visually separate setup, validation,
  processing, and output or cleanup so code does not form a continuous wall of text.

# Code documentation and naming

- Add a short comment for every function, method, class, struct,
  global variable, and similar declaration you introduce. Explain why
  it exists and summarize its behavior on success, edge cases, and
  failure. Document each returned error and exception that can
  propagate. State explicitly when there are no returned errors or
  exceptions.
- Comment key lexical blocks, including loops, conditionals, switch
  statements, and standalone scopes. Explain the purpose of the block
  or why a branch matters.
- Write every code comment in both English and Russian. Keep the two
  versions together and update them together.
- Use `Word_Word_Word` for classes and structs, `WORD_WORD` for
  global variables and constants, and `word_word` for ordinary
  variables, functions, and methods.

# Constants

- Extract values that define formats, application rules, defaults, or limits
  and may need to change into descriptive named constants instead of
  hardcoding them at use sites. Examples include metadata prefixes, filenames,
  directory names, format versions, sentinel values, and numeric limits.
- Reuse a shared constant wherever the same logical value is needed. Keep
  constants in their owning component and the narrowest appropriate scope.
  Expose a constant in a public header only when callers need it or it has a
  clear potential use for them. Keep implementation details in source files,
  regardless of whether their values are literals or computed expressions.
  Derive related values, such as prefix lengths, from their constants instead
  of duplicating them.
- Build diagnostics, help text, and other derived output from those same
  constants. Do not repeat their values inside string literals: validation
  rules and their descriptions must stay consistent when a constant changes.

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

- Test query syntax, ASTs, matching, and repository behavior directly
  through libmado. Use CLI integration tests for command dispatch, flags,
  exit codes, diagnostics, output formats, and representative end-to-end
  wiring. Keep detailed library cases in the library suites.
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
