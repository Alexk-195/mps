# MPS — Claude Guide

## Important Files

| File | Purpose |
|------|---------|
| `src/mps.h` | Public API — Pool, Worker, Message declarations |
| `src/mps.cpp` | Core implementation |
| `src/mps_message.h` | Message type definitions |
| `src/mps_synchronized.h` | Synchronized wrapper utility |
| `CMakeLists.txt` | Top-level build config (C++17, FetchContent for GoogleTest) |
| `tests/` | GoogleTest unit tests — one file per subsystem |
| `tutorials/mps_tutorials.cpp` | End-to-end usage examples (14 tutorials) |
| `.github/workflows/tests.yml` | CI workflow — builds and runs tests via ctest |

## Build & Test

```bash
cmake -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
