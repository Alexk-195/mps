# MPS — Claude Guide

MPS (Message Processing System) is a C++17 multithreading framework for embedded and concurrent applications. It abstracts thread management, mutexes, and condition variables behind a message-passing model built on three primitives: **pool** (thread + queue), **worker** (message handler), and **message** (data packet).

Version: `VERSION_MAJOR = 1`, `VERSION_MINOR = 11` (`src/mps.h:63-64`)

---

## Repository Layout

```
mps/
├── src/
│   ├── mps.h                  # Public API — all types, templates, interfaces
│   ├── mps.cpp                # Implementation of pool, worker, base, utilities
│   ├── mps_message.h          # mps::message base class
│   └── mps_synchronized.h     # mps::synchronized<T> template
├── tests/
│   ├── CMakeLists.txt         # GoogleTest via FetchContent; two executables
│   ├── support.h              # Shared test helpers (CountingWorker, etc.)
│   ├── test_timer.cpp
│   ├── test_ts_queue.cpp
│   ├── test_synchronized.cpp
│   └── test_object_tracking.cpp  # Built separately with MPS_TRACK_OBJECTS
├── tutorials/
│   ├── mps_tutorials.h        # Tutorial function declarations
│   └── mps_tutorials.cpp      # 14 runnable end-to-end examples
├── CMakeLists.txt             # Top-level build: static lib + tutorial + tests
├── build.sh                   # Legacy single-file build (debug only)
├── TODO.md                    # Known bugs and improvement proposals
└── .github/workflows/tests.yml  # CI: configure → build → ctest on ubuntu-latest
```

---

## Build & Test

```bash
cmake -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Build outputs:
- `build/libmps.a` — static library
- `build/mps_tutorial` — tutorial runner
- `build/mps_tests` — main test suite
- `build/mps_tests_tracking` — object-tracking tests (compiled with `-DMPS_TRACK_OBJECTS`)

`build.sh` is a legacy single-call g++ script that hardcodes `-DMPS_TRACK_OBJECTS -O0`. Prefer CMake.

---

## Architecture

### Core Triad

| Class | File | Role |
|-------|------|------|
| `mps::pool` | `src/mps.h:384` | Abstract interface; factory via `pool::create()`. Owns the OS thread and message queue. |
| `mps::pool_impl` | `src/mps.cpp` | Concrete pool implementation (PIMPL). Not used directly. |
| `mps::worker` | `src/mps.h:192` | Abstract base; users override `process()`. One instance can belong to at most one pool at a time. |
| `mps::message` | `src/mps_message.h:11` | Base for all message types. Passed as `shared_ptr<const message>`. |

### Supporting Types

| Class | File | Role |
|-------|------|------|
| `mps::pool_options` | `src/mps.h:330` | Configuration struct passed to `pool::create()`. |
| `mps::ts_queue<T>` | `src/mps.h:251` | Thread-safe FIFO queue with blocking pop and optional push limit. |
| `mps::timer` | `src/mps.h:415` | Stopwatch; `elapsed()` returns ms since last `reset()`. |
| `mps::waiter<T_m>` | `src/mps.h:437` | Worker that blocks until a message of type `T_m` arrives. |
| `mps::messagewaiter<T_m>` | `src/mps.h:526` | `waiter` variant that matches by pointer identity. |
| `mps::synchronized<T>` | `src/mps_synchronized.h:10` | Thread-safe deep-copy wrapper for sharing data across threads. |
| `mps::distributor` | `src/mps.h:554` | Wraps N pools; distributes workers and messages round-robin. |
| `mps::notification_message` | `src/mps.h:155` | Delivered to workers on timeout or `push_back_notification()`. |
| `mps::base` | `src/mps.h:126` | Common base for `pool` and `worker`; provides `node_name()` and optional instance tracking. |

### Interfaces

| Interface | Implementors | Methods |
|-----------|-------------|---------|
| `i_messages_acceptor` | `pool`, `distributor` | `push_back()`, `push_back_to_limit()` |
| `i_worker_pool` | `pool`, `distributor` | `add_worker()`, `remove_worker()` |
| `i_startable` | `pool`, `distributor` | `start()`, `stop()`, `join()` |

---

## Public API Reference

### Global Functions (`src/mps.h:71-80`)

```cpp
void mps::sleep_ms(unsigned long ms);
void mps::sleep_us(unsigned long us);
unsigned int mps::get_this_thread_prio();
void mps::set_this_thread_prio(unsigned int p);
```

### Pool Lifecycle

```cpp
auto p = mps::pool::create();                   // default options
auto p = mps::pool::create(pool_options{...});  // with options
p->start();
p->stop();   // signals stop; returns immediately
p->join();   // blocks until thread exits
```

### Workers and Messages

```cpp
p->add_worker(std::make_shared<MyWorker>());
p->push_back(std::make_shared<MyMessage>());
p->push_back_to_limit(msg, limit, pushed_out);
p->push_back_notification();  // delivers notification_message
p->flush(timeout_ms);         // returns true when queue drains
```

### pool_options Fields (`src/mps.h:330-358`)

| Field | Default | Meaning |
|-------|---------|---------|
| `timeout_wait_for_message` | `-1` (infinite) | ms to wait; delivers `notification_message` on timeout |
| `priority` | `100` | Locking hierarchy priority (not OS scheduling) |
| `type` | `T_NORMAL` | OS scheduling class (`T_NORMAL`, `T_LOWER_PRIO`, `T_HIGHER_PRIO`, `T_IDLE`) |
| `stop_if_no_workers` | `false` | Auto-stop when last worker is removed |

### Convenience Helper

```cpp
// Runs a function exactly once in a pool thread, then stops:
auto p = mps::pool_thread([]{ /* work */ }, "my-thread");
p->join();
```

---

## Key Patterns and Conventions

### Threading Safety Markers (`src/mps.h:45-47`)

These are documentation-only macros (no enforcement):

```cpp
#define mps_thread_safe       const     // callable from any thread
#define mps_thread_critical   mutable   // must be protected by a lock
#define mps_thread_critical_cast const_cast
```

Methods marked `mps_thread_safe` may be called from any thread. Members marked `mps_thread_critical` must only be accessed under the appropriate mutex.

### Locking Hierarchy and `waiter`

MPS prevents deadlocks through priority ordering. Every pool has an integer `priority` (default 100). A `waiter::wait()` call is only permitted from a thread whose `get_this_thread_prio()` is **strictly greater** than the owning pool's priority. Violating this throws `mps::locking_exception`.

```cpp
// Safe: main thread (default prio 0) cannot wait on a prio-100 pool.
// Set calling thread to prio 200 first:
mps::set_this_thread_prio(200);
auto msg = my_waiter->wait(1000);  // ok, 200 > 100
```

### Worker Safety

`worker::process()` is non-`const`: workers may mutate their own state inside `process()`. That state is safe to access only from within the owning pool's thread (i.e., from `process()`). Never access worker state directly from other threads without external synchronization.

### Factory Pattern

`pool` and `distributor` cannot be constructed directly. Always use the static factory:

```cpp
auto p = mps::pool::create();
auto d = mps::distributor::create(4, opts, "pool-name");
```

### RAII Lifecycle

Wrap pools in `shared_ptr` and always call `stop()` + `join()` before destruction. In tests, use `make_started_pool()` from `tests/support.h` which does this automatically via a custom deleter.

---

## Test Structure

Tests use **GoogleTest** fetched via CMake `FetchContent` at `v1.14.0`.

### Two Test Executables

| Binary | Sources | Notes |
|--------|---------|-------|
| `mps_tests` | `test_timer.cpp`, `test_ts_queue.cpp`, `test_synchronized.cpp` | Standard build |
| `mps_tests_tracking` | `test_object_tracking.cpp` | Built with `-DMPS_TRACK_OBJECTS` |

### Test Helpers (`tests/support.h`)

| Helper | Purpose |
|--------|---------|
| `CountingWorker` | Atomically counts `process()` calls; `await(n, timeout_ms)` blocks until count ≥ n |
| `RecordingWorker<T>` | Records all messages of type T in a thread-safe vector; has `await(n, ms)` |
| `ScopedThreadPrio(prio)` | RAII: sets then restores calling thread's mps priority |
| `make_started_pool(opts)` | Returns an already-started pool that auto-stops/joins on scope exit |

### Test Conventions

- Every test that starts a pool **must** `stop()` + `join()` before the test returns — use `make_started_pool()` to guarantee this.
- Never use bare `sleep_ms()` for synchronization in tests; use `flush()`, latches, or the `await()` helpers.
- Timing-sensitive assertions use generous bounds (≥ requested timeout, ≤ 5× requested) to avoid CI flakiness.
- Tests in `test_object_tracking.cpp` are the only ones that may assume `MPS_TRACK_OBJECTS` is defined.

---

## CI Pipeline (`.github/workflows/tests.yml`)

Triggers on push and PR to `main`. Single job on `ubuntu-latest`:

1. `sudo apt-get install -y cmake g++`
2. `cmake -B build -DMPS_BUILD_TESTS=ON`
3. `cmake --build build --parallel`
4. `ctest --test-dir build --output-on-failure`

No Windows/macOS matrix, no sanitizers, no coverage — these are listed in `TODO.md` as future improvements.

---

## Optional: Object Tracking (`MPS_TRACK_OBJECTS`)

When compiled with `-DMPS_TRACK_OBJECTS`, every `pool` and `worker` instance registers itself in a global set on construction and deregisters on destruction.

```cpp
mps::base::dump_all_instances(std::cout);  // prints all live instances
```

Use this to detect leaks during development. The feature is disabled in normal builds because it adds synchronization overhead and the hard-exit in `base::~base()` (see known issues below) is not production-safe.

---

## Known Issues (see `TODO.md` for full details)

| Issue | Location | Severity |
|-------|----------|---------|
| Linux platform detection uses `_linux` (single underscore) instead of `__linux__` | `src/mps.cpp:14` | High — falls through to POSIX only via `__unix__` |
| `base::~base()` calls `exit(-2)` on double-free when tracking | `src/mps.cpp:315` | Medium — hostile to embedders |
| `add_worker` sets `owned = true` before `owner_pool`; tiny race window | `src/mps.cpp:466` | Low |
| `insufficient_privileges` writes to `std::cout` instead of `std::cerr` | `src/mps.cpp:187-189` | Low |
| Shared `nmessage` reused for all notifications (safe but undocumented) | `src/mps.cpp:421,599` | Info |
| Signed/unsigned mix in `waiter::check` | `src/mps.h:446-448` | Info |

---

## Platform Support

Platform detection lives in `src/mps.cpp`. Three implementations:

| Macro | Platform | Features |
|-------|----------|---------|
| `MPS_THREADS_POSIX` | Linux / `__unix__` | pthreads, `SCHED_RR`, `gettid()`, `pthread_setname_np` |
| `MPS_THREADS_MSVC` | Windows | Windows API, `SetThreadDescription` |
| `MPS_THREADS_QT` | Qt | Stub (unimplemented) |
| (none) | Unknown | No-op stubs |

---

## Typical Usage Pattern

```cpp
// 1. Define messages
struct MyMsg : mps::message { int value; };

// 2. Define a worker
class MyWorker : public mps::worker {
    void process(std::shared_ptr<const mps::message> m) override {
        if (auto typed = std::dynamic_pointer_cast<const MyMsg>(m))
            std::cout << typed->value << "\n";
    }
};

// 3. Wire up and run
auto p = mps::pool::create();
p->start();
p->add_worker(std::make_shared<MyWorker>());

auto msg = std::make_shared<MyMsg>();
msg->value = 42;
p->push_back(msg);

p->stop();
p->join();
```

See `tutorials/mps_tutorials.cpp` for 14 progressively complex examples covering `waiter`, `distributor`, `pool_thread`, locking hierarchy, and more.
