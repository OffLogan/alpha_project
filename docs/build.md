# Build Guide

## Requirements
- C++ compiler compatible with the project toolchain
- CMake
- Qt 6 with Widgets support

## Build on macOS
Typical flow:

```bash
cmake -S . -B build
cmake --build build
```

If you use Qt Creator:
- open the project root
- configure a Qt 6 kit
- run CMake
- build the app target

## Run
After building, launch the generated app bundle or executable from your build directory.

## Test
The project contains smoke tests.
Typical flow:

```bash
cmake --build build --target alpha_project_smoke_tests
ctest --test-dir build
```

## Notes
- The app stores user data in the writable Qt application data location.
- If you do not see the expected data from the repo `data/` folder, the app may already be using the per-user app data directory instead.
- Windows packaging is planned for a later release and should be documented here once the deploy flow is stable.

