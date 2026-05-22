# MPS Project Review — Findings and Test Proposal

Review of the MPS (Message Processing System) C++ multithreading framework
(`src/mps.*`, `tutorials/mps_tutorials.*`).

## 1. Project Overview

| Area | Status |
| --- | --- |
| Library code | `src/mps.h`, `src/mps.cpp`, `src/mps_message.h`, `src/mps_synchronized.h` |
| Demo / smoke runner | `tutorials/mps_tutorials.cpp` (14 tutorials wrapped in a `main`) |
| Build | `CMakeLists.txt` (builds `mps_tutorial` only) + `build.sh` (single g++ call) |
| Unit tests | **None** |
| CI | **None** |
| Static analysis / formatter config | **None** (no `.clang-format`, `.clang-tidy`, `.editorconfig`) |

## 2. Findings

### 2.1 Test infrastructure
- **No unit tests.** The tutorial binary runs end-to-end scenarios but contains
  no assertions; failures are detected only by visual inspection of stdout.
- **No CI workflow.** Nothing prevents regressions on push / PR.
- **`CMakeLists.txt` exposes only the tutorial target.** No `enable_testing()`,
  no `add_subdirectory(tests)`, no install target, no library target that
  downstream projects could `target_link_libraries(... mps)`.
- **`build.sh` hardcodes `-DMPS_TRACK_OBJECTS` and `-O0`.** Useful for debug,
  but the project never validates a release build.

### 2.2 Bugs / correctness
- **Linux platform detection is broken.** `src/mps.cpp:14`
  `#if defined(_linux) || defined(__unix__)` uses `_linux` (with a single
  underscore), which no compiler defines. The intended macro is `__linux__`.
  The branch is currently selected only via `__unix__`, which is not defined
  on every glibc toolchain. Risk: silent fall-through to the
  "Unknown platform" stub on some configurations.
- ~~**`add_worker` has a small race window.**~~ Fixed: `remove_worker` now
  checks the atomic `owned` flag instead of locking `owner_pool`. Since
  `add_worker` sets `owned` before enqueueing the m_add_worker message and
  `remove_worker_internal` clears it only after erasing the worker from the
  list, the (owned, owner_pool) ordering can no longer cause a spurious
  "worker has no owner" throw or a torn `weak_ptr` read.
- **`base::~base` calls `exit(-2)`** when tracking detects double-free
  (`src/mps.cpp:315`). Hard-exit from a destructor makes the framework
  unfriendly to embedders; throw or log and continue instead.
- **`insufficient_privileges` error path writes to `std::cout`** instead of
  `std::cerr` (`src/mps.cpp:187-189`).
- **Notification message instance is shared and reused.** `pool_impl::nmessage`
  is a single `shared_ptr` reused for every notification (`src/mps.cpp:421,
  599`). Safe today because `message` is const, but worth a comment so future
  changes do not break it.
- **Signed/unsigned mix in `waiter::check`.** `src/mps.h:446-448` assigns
  `unsigned int` priority into `int owner_prio` / `int caller_prio` to enable
  the `<=` comparison; with `-Wconversion` this would warn. Use `unsigned`.

### 2.3 API / design observations
- **`mps::synchronized` only exposes deep copy semantics.** No
  `read([](const T&){...})` / `write(...)` callback variants, so callers must
  copy entire objects on every read. Consider adding `with_lock(callable)`.
- **`pool::flush` swallows the source of failure.** Returns only `bool`, no
  way to distinguish timeout from "pool is stopping".
- **`distributor::add_worker` cannot target a specific pool.** Workers are
  always placed round-robin, which can break callers that need affinity.
- **`pool_thread` immediately calls `stop()` after `push_back_notification()`.**
  Works because messages drain before the stop sentinel, but the contract is
  subtle; document it or rename the helper.
- **`worker::process` is non-`const`** while messages are `const`. Worker
  authors often want to mutate per-worker state — fine, but worth noting that
  this state is only safe to read from the owning pool's thread.

### 2.4 Documentation
- `README.md` lists Pool / Worker / Message but omits `waiter`, `timer`,
  `distributor`, `pool_thread`, `synchronized`. Add a short feature table.
- README claims build artifacts land in `build/`, but `build.sh` writes
  `./mps_tutorial` in the repo root. Align the two.
- Header docstrings contain typos: `"working file"` → `"working fine"`
  (`tutorials/mps_tutorials.cpp:32`), `"circualar"` → `"circular"`
  (`tutorials/mps_tutorials.cpp:527`), `"Plattform"` → `"Platform"`
  (`src/mps.h:352`).

### 2.5 Build / packaging
- No exported CMake package (`mpsConfig.cmake`), no `INTERFACE`/`STATIC`
  library target; downstream users have to copy sources.
- `CMakeLists.txt` does not set warning flags. `build.sh` does, but the two
  paths diverge.
- `.gitignore` does not cover `build/`, IDE folders (`.vscode/`, `.idea/`),
  or `compile_commands.json`.

## 3. Unit Test Proposal

### 3.1 Framework choice
Use **GoogleTest** via CMake `FetchContent` — no system install needed,
single dependency, integrates with `ctest`. Fallback option: Catch2 (header
only) if a hard "no network at build" rule is required.

### 3.2 Layout
```
tests/
  CMakeLists.txt
  test_main.cpp           # gtest main (only if custom main needed)
  test_timer.cpp
  test_ts_queue.cpp
  test_synchronized.cpp
  test_message.cpp
  test_pool_lifecycle.cpp
  test_pool_workers.cpp
  test_pool_messages.cpp
  test_waiter.cpp
  test_distributor.cpp
  test_pool_thread.cpp
  test_priority.cpp
  test_object_tracking.cpp # compiled with -DMPS_TRACK_OBJECTS
```

Each file stays under ~150 lines. Helpers (counting worker, recording
worker, latch) live in a single `tests/support.h`.

### 3.3 Suggested test cases

| Suite | Test | What it asserts |
| --- | --- | --- |
| `Timer` | `ElapsedIsMonotonic` | `elapsed()` only increases between calls. |
| `Timer` | `ResetReturnsToZero` | After `reset()`, `elapsed() < small epsilon`. |
| `Timer` | `ConstructorWithoutResetStaysFrozen` | `timer(false)` reports 0 until `reset()`. |
| `TsQueue` | `PushPopFifo` | Items come out in insertion order. |
| `TsQueue` | `PopTimeoutReturnsEmpty` | `pop(item, 10)` on empty queue returns `(0, false)`. |
| `TsQueue` | `PushToLimitRespectsLimit` | After reaching `limit`, `pushed == false`. |
| `TsQueue` | `PushWithClearDropsExisting` | `push(item, true)` empties the queue first. |
| `TsQueue` | `BlockingPopWakesOnPush` | Consumer thread blocked on `pop(-1)` returns once producer pushes. |
| `Synchronized` | `RoundTripCopy` | `copy_from` then `copy_to` yields identical data. |
| `Synchronized` | `ConcurrentReadersAndWriter` | Stress 4 readers + 1 writer for 100 ms, no torn reads. |
| `Message` | `DerivedTypeCastsBackThroughBase` | `dynamic_pointer_cast` recovers the derived type. |
| `PoolLifecycle` | `StartStopJoin` | A pool can be started, stopped, joined without messages. |
| `PoolLifecycle` | `DoubleStartThrows` | Second `start()` throws `mps::exception`. |
| `PoolLifecycle` | `StopIfNoWorkers` | `pool_options::stop_if_no_workers` stops pool after last `remove_worker`. |
| `PoolLifecycle` | `NotificationTimeoutDeliversNotification` | With `timeout_wait_for_message = 50`, worker receives `notification_message` at least once. |
| `PoolWorkers` | `AddNullWorkerThrows` | `add_worker(nullptr)` throws. |
| `PoolWorkers` | `AddSameWorkerTwiceThrows` | Adding the same `shared_ptr<worker>` to two pools throws on the second. |
| `PoolWorkers` | `RemoveFromWrongPoolThrows` | `remove_worker` on a pool that does not own the worker throws. |
| `PoolWorkers` | `WorkerExceptionTriggersRemoval` | A worker that throws is removed and subsequent messages are not delivered to it. |
| `PoolMessages` | `MessagesArriveInOrder` | A single worker receives N typed messages in push order. |
| `PoolMessages` | `MultipleWorkersAllReceiveMessage` | Two workers in the same pool both see the same message. |
| `PoolMessages` | `PushBackToLimitRejectsWhenFull` | At capacity, `pushed == false`. |
| `PoolMessages` | `FlushReturnsTrueWhenDrained` | `flush(1000)` after a short workload returns `true`. |
| `PoolMessages` | `FlushReturnsFalseOnTimeout` | Worker that sleeps longer than the timeout makes `flush` return `false`. |
| `Waiter` | `WaitReceivesMatchingMessage` | `waiter<T>::wait(timeout)` returns the pushed message. |
| `Waiter` | `WaitIgnoresOtherTypes` | A push of an unrelated message does not satisfy the wait. |
| `Waiter` | `LockingExceptionWhenCallerPrioTooLow` | Main thread with prio 0 waiting on prio-100 pool throws `locking_exception` carrying both names/priorities. |
| `Waiter` | `ResetAllowsNextMessage` | After `reset()`, the next matching message is returned. |
| `Waiter` | `MessageWaiterMatchesByIdentity` | `messagewaiter` returns only the exact pointer it was constructed with. |
| `Distributor` | `RoundRobinAcrossPools` | With N pools and N×K identical workers, K messages reach each worker. |
| `Distributor` | `RemoveUnknownWorkerReturnsZero` | `remove_worker` of an outsider returns 0 (does not throw). |
| `PoolThread` | `RunsAndJoins` | Lambda is executed exactly once, `join()` returns. |
| `Priority` | `GetSetRoundTrip` | `set_this_thread_prio(N)` followed by `get_this_thread_prio()` returns `N`. |
| `Priority` | `PoolPropagatesPriorityToTls` | Inside `process()`, `get_this_thread_prio()` equals `options.priority`. |
| `ObjectTracking` | `LeakedPoolIsReported` | Built with `MPS_TRACK_OBJECTS`; intentionally leaked pool shows up in `dump_all_instances`. |
| `ObjectTracking` | `DestroyedPoolIsForgotten` | After scope exit, `dump_all_instances` reports "no instances left". |

### 3.4 Test helpers (tests/support.h)
- `CountingWorker` — atomic counter incremented per `process()`, exposes
  `await(n, timeout)` built on `std::condition_variable`.
- `RecordingWorker<T>` — pushes received messages into a thread-safe vector.
- `ScopedThreadPrio` — RAII for `set_this_thread_prio` to keep tests
  independent.
- `make_started_pool(opts)` — convenience factory that returns a pool already
  started and adds a `Cleanup` guard that `stop()` + `join()` on destruction.

### 3.5 Conventions
- Every test that starts a pool must `stop()` and `join()` before returning,
  via RAII (see `make_started_pool`). No bare `sleep_ms` for synchronization —
  use latches / `flush()`.
- Timing-sensitive assertions use generous bounds (≥ requested timeout,
  ≤ 5× requested) so CI noise does not cause flakes.
- One test per behaviour, named `Should...` or `<Action><ExpectedResult>`.
- No reliance on `MPS_TRACK_OBJECTS` except in `test_object_tracking.cpp`,
  which is compiled into a separate binary with the define enabled.

### 3.6 CMake integration sketch
- Promote `src/mps.cpp` into a `mps` static library target.
- Add `option(MPS_BUILD_TESTS "Build unit tests" ON)`.
- `tests/CMakeLists.txt` fetches GoogleTest, builds one `mps_tests`
  executable, and a second `mps_tests_tracking` with
  `target_compile_definitions(... PRIVATE MPS_TRACK_OBJECTS)`.
- `enable_testing()` + `gtest_discover_tests(...)` so `ctest` works.

### 3.7 CI proposal (out of scope but recommended)
- GitHub Actions: matrix on `ubuntu-latest` + `windows-latest`, GCC/Clang/MSVC,
  Debug + Release. Run `ctest --output-on-failure`.
- Add `-fsanitize=thread` job for the Linux Debug build (the framework is
  thread-heavy and TSan is the most valuable check).

## 4. Priority Order

1. Fix Linux platform detection (`_linux` → `__linux__`). One-line bug.
2. Introduce `mps` library target in CMake; keep `mps_tutorial` linking
   against it. Pre-requisite for testing.
3. Land the test scaffolding (`tests/`, GoogleTest via FetchContent,
   `test_timer`, `test_ts_queue`, `test_synchronized` first — these have no
   threading complexity).
4. Add pool / worker / waiter / distributor suites.
5. Add CI workflow.
6. Address remaining design notes (synchronized callback API, distributor
   targeted add, README updates).
