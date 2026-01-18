# Contributing

## Build

```sh
cmake -S . -B build
cmake --build build --config Release
```

## Tests

```sh
ctest --test-dir build -C Release --output-on-failure
```

- E2E tests require the Copilot CLI (`copilot`) to be installed and authenticated.
- Skip E2E tests in CI/non-interactive runs: set `COPILOT_SDK_CPP_SKIP_E2E=1`.

## Style

- Format C++ with `.clang-format` (4 spaces, Allman braces):
  - `clang-format -i src/*.cpp include/copilot/*.hpp examples/*.cpp tests/*.cpp`

## Pull Requests

- Use clear PR descriptions, include test output, and note any API/behavior changes.
- Prefer Conventional Commits-style messages (e.g., `feat(tests): ...`, `fix(example): ...`).
