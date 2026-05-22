# Message Processing System (MPS)

[![Tests](https://github.com/Alexk-195/mps/actions/workflows/tests.yml/badge.svg)](https://github.com/Alexk-195/mps/actions/workflows/tests.yml)

MPS is a C++17 framework for multithreading in embedded and concurrent applications. It abstracts thread management, mutexes, and condition variables behind a message-passing model built on three core primitives:

- **Pool** (`mps::pool`) — owns an OS thread and a thread-safe message queue.
- **Worker** (`mps::worker`) — abstract message handler; override `process()` and attach to a pool.
- **Message** (`mps::message`) — base for all message types; passed as `shared_ptr<const message>`.

Additional components:

| Component | Purpose |
|-----------|---------|
| `mps::waiter<T>` | Blocks the caller until a message of type `T` arrives. |
| `mps::timer` | Stopwatch; `elapsed()` returns ms since last `reset()`. |
| `mps::distributor` | Wraps N pools and distributes workers/messages round-robin. |
| `mps::pool_thread` | Convenience helper that runs a lambda once in a pool thread. |
| `mps::synchronized<T>` | Thread-safe deep-copy wrapper for sharing data across threads. |
| `mps::ts_queue<T>` | Thread-safe FIFO queue with blocking pop and optional push limit. |

For detailed information, refer to `src/mps.h` and `tutorials/mps_tutorials.cpp` (14 runnable examples).

## Building MPS

You can build the MPS project using either CMake or the provided `build.sh` script.

### Using CMake

```bash
git clone https://github.com/Alexk-195/mps.git
cd mps
cmake -B build
cmake --build build --parallel
```

Build outputs in `build/`:
- `libmps.a` — static library
- `mps_tutorial` — tutorial runner (14 examples)
- `mps_tests` — unit test suite
- `mps_tests_tracking` — object-tracking tests (compiled with `-DMPS_TRACK_OBJECTS`)

### Running Tests

```bash
ctest --test-dir build --output-on-failure
```

### Using the `build.sh` Script

`build.sh` is a legacy single-call g++ script useful for quick debug builds. It hardcodes `-DMPS_TRACK_OBJECTS -O0` and outputs `mps_tutorial` in the repo root. Prefer CMake for all other use cases.

```bash
chmod +x build.sh
./build.sh
```

## License

This project is licensed under the MIT License. For more details, see the [LICENSE](https://github.com/Alexk-195/mps/blob/main/LICENSE) file in the repository.

---

For any questions or contributions, feel free to open an issue or submit a pull request on the [GitHub repository](https://github.com/Alexk-195/mps). 
