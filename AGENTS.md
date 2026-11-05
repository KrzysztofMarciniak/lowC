# Repository Guidelines

These instructions apply to the entire repository.

## Project layout

- `src/` contains the C99 compiler, organized by compiler subsystem.
- `stdlib/` contains lowC public and internal modules.
- `tests/smoke/examples/` contains smoke-test programs; `tests/suites/` contains focused suites and fixtures; shared test helpers are in `tests/support/`.
- `README.md` documents the language, compiler usage, and standard library.

## Build and test

- `make` builds the optimized compiler at `build/lowc`.
- `make debug` builds the debug compiler at `build/debug/lowc`.
- Release compilation uses C99 and `-Wall -Wextra -Wpedantic`, optimization (`-Os`, `-flto`), function/data sections, and disabled unwind tables. The default link flags are `-Wl,--gc-sections -s`.
- `make debug` uses the warning and C99 flags with `-g`, without release optimization or linker flags. `make portable` and `make static` add static linking; `make tcc` uses `tcc` and its `-s` linker flag.
- `make test` runs smoke tests and all focused suites.
- `make test TEST=<suite>` runs one area, such as `frontend`, `modules`, `semantic`, or `standard-library`. Use `TEST=smoke` for smoke examples only.

Run the narrowest relevant test target after changes. Tests require a C compiler driver (`cc` by default).

## Contribution guidance

- Follow the surrounding C99 style and keep subsystem interfaces consistent with their existing `public` and `internal` files.
- Every lowC variable declaration must have an initializer; use `= 0` when zero-initialization is intended, including arrays and structs.
- Import lowC module names explicitly with `import path.lowc : name;`. The `public/` import prefix resolves beneath the standard-library root; `LOWC_STDLIB_DIR` can override its runtime location.
- Keep test fixtures focused on the behavior under test and use the existing suite runner/helper patterns.
- Update `README.md` when changing documented language or user-facing compiler behavior.
