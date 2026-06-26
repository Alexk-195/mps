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

## Quick Start

```cpp
#include "mps.h"
#include <iostream>
#include <memory>
#include <string>

// 1. Define a message — data only, no logic. Sent as shared_ptr<const message>.
struct GreetMessage : mps::message {
    std::string text;
};

// 2. Define a worker — override process() with your handling logic.
class GreetWorker : public mps::worker {
    void process(std::shared_ptr<const mps::message> m) override {
        if (auto g = std::dynamic_pointer_cast<const GreetMessage>(m))
            std::cout << g->text << '\n';
    }
};

int main() {
    auto pool = mps::pool::create();                   // thread + message queue
    pool->add_worker(std::make_shared<GreetWorker>());
    pool->start();                                     // launch the worker thread

    auto msg = std::make_shared<GreetMessage>();
    msg->text = "Hello from an MPS worker!";
    pool->push_back(msg);                              // enqueue work

    pool->stop();   // stop is queued after msg, so msg is processed first (FIFO)
    pool->join();   // block until the thread has finished
}
```

Compile against the static library (or just add `src/mps.cpp` to your build):

```bash
g++ -std=c++17 -Isrc main.cpp src/mps.cpp -o demo -lpthread
```

## Key Concepts

- **Pool = thread + queue.** Each pool owns exactly one OS thread that pops messages and dispatches them to its workers. Use multiple pools (or a `distributor`) for parallelism.
- **FIFO ordering within a pool.** Messages are processed in the order they were pushed; a later message is never processed before an earlier one. (Across a `distributor`, order is *not* guaranteed.)
- **Messages are immutable.** They are handed to `process()` as `shared_ptr<const mps::message>`, so the same instance can be shared between threads without copying or locking.
- **Worker state is isolated.** A worker's data is only touched by its own pool thread, so most `process()` code needs no locks of its own.
- **Lifecycle.** `create()` → `add_worker()` → `start()` → `push_back()` … → `stop()` → `join()`. Wrap pools in `shared_ptr` and always `stop()` + `join()` before destruction.
- **Deadlock-resistant waiting.** `mps::waiter<T>` blocks until a matching message arrives, and `pool::flush()` blocks until the queue drains. Both enforce a **locking hierarchy**: the calling thread's locking priority must be *strictly greater* than the target pool's `priority` (default `100`), otherwise an `mps::locking_exception` is thrown. Raise it with `mps::set_this_thread_prio()`; never wait on your own pool from inside `process()`.

## When to Use MPS

**A good fit for:**

- Event-driven, actor-style, or pipeline designs where components communicate by passing messages.
- Background/worker threads in embedded and desktop applications where you want to avoid hand-rolled mutexes and condition variables.
- Concurrency that benefits from clear ownership and isolation rather than shared mutable state.

**Consider something else for:**

- Data-parallel number crunching that needs a work-stealing scheduler or one task fanned out across all cores.
- Workloads requiring per-message priorities (the queue is strict FIFO).
- Hard real-time scheduling without the OS privileges needed for real-time thread classes.

## Advantages

- **Minimal dependencies** — only the C++17 standard library and the system threads library. Ships as a static lib (`libmps.a`) plus headers; nothing to vendor.
- **No manual synchronization** — you write `process()`; the framework owns the threads, queue, mutexes, and condition variables.
- **Deadlock-resistant by design** — the `waiter` locking hierarchy turns latent circular waits into an immediate, explicit `locking_exception`.
- **Predictable** — strict FIFO processing per pool and explicit `start`/`stop`/`join` lifecycle.
- **Portable threading details** — cross-platform thread naming and OS scheduling class selection on POSIX and Windows.
- **Batteries included** — `timer`, `ts_queue<T>`, `synchronized<T>`, `distributor`, and `pool_thread` helpers.
- **Leak diagnostics** — optional object tracking (`MPS_TRACK_OBJECTS`) reports pools/workers that were never released.

## Limitations & Gotchas

- **`priority` is a *locking-hierarchy* priority, not an OS scheduling priority.** It only governs who may `wait()` on whom. The OS scheduling class is configured separately via `pool_options::type` (`T_NORMAL`, `T_LOWER_PRIO`, `T_HIGHER_PRIO`, `T_IDLE`).
- **Blocking helpers require ranking.** `waiter::wait()` and `pool::flush()` throw `mps::locking_exception` unless the caller out-ranks the pool (see Key Concepts). Calling `flush()` from an ordinary thread therefore needs a prior `mps::set_this_thread_prio()`.
- **One pool per worker.** A worker may belong to at most one pool at a time; move it by removing it first, then adding it elsewhere.
- **Worker exceptions are terminal for that worker.** An exception escaping `process()` is logged to `stderr` and the worker is removed from its pool; it does not bring down the thread or the process.
- **One pool = one thread.** Processing inside a pool is single-threaded. Scale out with more pools or a `distributor` (which trades ordering guarantees for round-robin parallelism; its `remove_worker` may return `0` if the worker is not one of its pools').
- **Elevated scheduling needs privileges.** `T_HIGHER_PRIO` uses real-time `SCHED_RR` and requires sufficient OS privileges; without them the failure is reported and the thread continues at default scheduling.
- **Platform coverage.** POSIX (Linux/Unix) and Windows are implemented; the Qt backend is a stub and unknown platforms fall back to no-op thread naming/scheduling (the message-passing core still works).

## Building MPS

You can build the MPS project using either CMake or the provided `build.sh` script.

**Requirements:** a C++17 compiler (GCC, Clang, or MSVC), a threads library (pthreads on POSIX, linked automatically), and CMake ≥ 3.24 for the CMake build. The unit tests fetch GoogleTest `v1.14.0` via CMake `FetchContent` when a system install is not found.

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

> **ThreadSanitizer note.** The timed-wait paths use `std::condition_variable::wait_for`/`wait_until` with a steady clock. GCC 11's bundled ThreadSanitizer lacks a `pthread_cond_clockwait` interceptor and reports *false positives* there (a plain `std::condition_variable` handoff reproduces them with no MPS code involved). The synchronization itself is correct — use Clang or GCC ≥ 12 to sanitize cleanly. AddressSanitizer and UndefinedBehaviorSanitizer are unaffected.

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
