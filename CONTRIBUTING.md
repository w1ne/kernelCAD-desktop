# Contributing to kernelCAD

## Branch Workflow (Trunk-Based)

- `main` — single branch, always deployable
- Feature branches → PR → `main`
- No develop branch, no release branches

## Making Changes

1. Branch from `main`: `git checkout -b feature/my-feature`
2. Make changes
3. Run all tests:
   ```bash
   cmake --build build -j$(nproc)
   cd build && ctest
   cd .. && ./tests/test_integration.sh
   ./tests/test_stress.sh
   ```
4. Push and create PR into `main`
5. CI runs automatically — merge when green

## Release Process

1. Update version in `CMakeLists.txt` and `CHANGELOG.md`
2. Tag on main: `git tag v0.2.0 && git push --tags`
3. GitHub Actions auto-builds and creates the GitHub Release

## Adding a New Tool

1. Add kernel method to `OCCTKernel.h/.cpp` (if new geometry operation)
2. Create feature class in `src/features/` (params struct + execute)
3. Add `Document::addXxx()` method
4. Add `AddXxxCommand` in `Commands.h/.cpp`
5. Register tool in `src/ui/ToolRegistration.cpp` — appears everywhere automatically
6. Add CLI command in `ScriptEngine.cpp`
7. Add integration test in `tests/test_integration.sh`
8. All 119+ tests must pass

## Code Style

- C++17, no C++20 features
- 4-space indentation
- `#pragma once` for headers
- Pimpl for OCCT isolation
- `std::make_unique` over raw `new`
- Descriptive OCCT error messages in catch blocks
