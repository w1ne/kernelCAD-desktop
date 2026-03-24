# Contributing to kernelCAD

## Branch Workflow

- `main` — protected, releases only (via PR)
- `develop` — integration branch, PRs merge here
- `feature/*` — feature branches, PR into develop
- `release/v*` — release candidates, PR into main

## Making Changes

1. Branch from `develop`: `git checkout -b feature/my-feature develop`
2. Make changes
3. Run all tests:
   ```bash
   cmake --build build -j$(nproc)
   cd build && ctest
   cd .. && ./tests/test_integration.sh
   ./tests/test_stress.sh
   ```
4. Push and create PR into `develop`

## Release Process

1. Create `release/vX.Y.Z` branch from `develop`
2. Update version in `CMakeLists.txt` and `CHANGELOG.md`
3. PR into `main`
4. Tag: `git tag v0.1.0 && git push --tags`
5. GitHub Actions builds and creates the release

## Adding a New Tool

1. Add kernel method to `OCCTKernel.h/.cpp` (if new geometry operation)
2. Create feature class in `src/features/` (params struct + execute)
3. Add `Document::addXxx()` method
4. Add `AddXxxCommand` in `Commands.h/.cpp`
5. Register tool in `src/ui/ToolRegistration.cpp`
6. Add CLI command in `ScriptEngine.cpp`
7. Add integration test in `tests/test_integration.sh`
8. All tests must pass

## Code Style

- C++17, no C++20 features
- 4-space indentation
- `#pragma once` for headers
- Pimpl for OCCT isolation
- `std::make_unique` over raw `new`
- Descriptive OCCT error messages in catch blocks
