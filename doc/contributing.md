# Contributing to BuildCache

BuildCache is an open source project, and anyone is of course welcome to
contribute to it.

Use [GitLab issues](https://gitlab.com/bits-n-bites/buildcache/-/issues) to report
any problems with BuildCache, or to request new features.

Use the [GitLab forking workflow](https://docs.gitlab.com/ee/user/project/repository/forking_workflow.html)
to suggest patches to the project.

## Coding conventions

### Language

BuildCache is written in C++11. Use it.

### Code formatting

The code is formatted using
[clang-format](https://clang.llvm.org/docs/ClangFormat.html) (the formatting
rules are given in [.clang-format](../src/.clang-format)). Use it.

### Miscellaneous rules

In general, try to follow the style of the existing code base.

#### Naming

* Use `snake_case` for most things (variables, types/classes, functions, ...).
* Use `SHOUTY_CASE` for compile-time constants.
* Types are suffixed with `_t` (e.g. `program_wrapper_t`).
* Class members are prefixed with `m_` (e.g. `int m_count;`).

#### Comments

* Comments should use proper sentences, with capitalization and an ending
  period.
* Use a blank line before comment lines.
* Document functions and interfaces with
  [Doxygen](http://www.doxygen.nl/manual/docblocks.html) comments (use the
  `/// @foo` style).

#### Includes

* Include what you use.

## Git history conventions

Use [the recipe model](https://www.bitsnbites.eu/git-history-work-log-vs-recipe/)
for Git branches and commits. In short:

* Use a feature branch for each patch.
* Use rebase (not merge) to update a branch against master.
* Use proper commit messages:
  - Follow the [Git conventions](https://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html).
  - Use the imperative mood ("Fix bug", *not* "Fixed bug").
  - Use proper capitalization.

## Portability

All code is written in a fully portable manner. There are CI pipelines in place
that ensure that BuildCache will build and run on at least Linux, macOS and
Windows, but the goal is that BuildCache should work on most POSIX-compatible
systems.

Any differences between systems are abstracted in suitable modules. For an
example, see [base/file_utils.cpp](../src/base/file_utils.cpp).

