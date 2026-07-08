# Design Document: Traceline

**A deterministic, safety-conscious multi-sensor fusion and flight-data recorder engine, built in modern C++23.**

## 1. Problem Statement

Avionics and safety-critical embedded systems fuse noisy, asynchronous sensor streams (IMU, GPS, barometric altimeter) into a single trustworthy state estimate, under hard real-time constraints, with an auditable record of what the system believed and when. Most portfolio-grade C++ projects ignore the constraints that make this hard: bounded latency, no unbounded memory growth, and traceability from requirement to implementation to test.

Traceline is a from-scratch simulation of this problem. It is not a toy Kalman filter demo — it is built to the discipline (not the certification) of DO-178C-adjacent engineering: written requirements, coding standard enforcement, deterministic hot paths, and a black-box-style flight recorder.

## 2. Goals / Non-Goals

**Goals**
- Deterministic, bounded-latency sensor fusion loop (target: p99.9 < 500µs per fusion cycle on commodity hardware)
- Zero dynamic allocation in the hot path after startup
- Strict subset of MISRA C++ conventions, enforced by clang-tidy in CI
- Full requirements-to-test traceability (see `docs/requirements.md`)
- Flight recorder module: append-only, crash-safe telemetry log
- Explicit, documented AI-assisted development workflow (see `docs/ai-workflow.md`)

**Non-Goals**
- Actual DO-178C certification (this is a portfolio project, not a certified artifact — the README says so explicitly, no overclaiming)
- Physical hardware integration (sensors are simulated with injectable noise/fault models)
- Distributed/multi-node operation (single-process scope, v1)

## 3. Architecture

```
                 ┌──────────────────┐
   Simulated     │   Sensor Layer    │  IMU / GPS / Baro simulators
   Sensors  ───▶ │  (fault-injectable)│  each with its own noise model
                 └────────┬──────────┘
                          │ SensorSample<T> (fixed-size, no heap)
                          ▼
                 ┌──────────────────┐
                 │  Fusion Engine    │  Kalman Filter (constant-acceleration model)
                 │  (deterministic)  │  bounded-time predict/update
                 └────────┬──────────┘
                          │ StateEstimate (timestamped)
              ┌───────────┴────────────┐
              ▼                        ▼
     ┌────────────────┐      ┌──────────────────┐
     │ Telemetry Bus    │      │ Flight Recorder   │
     │ (lock-free SPSC) │      │ (append-only log) │
     └────────────────┘      └──────────────────┘
              │
              ▼
        Downstream consumers
        (health monitor, replay tool)
```

**Module boundaries** (see `include/traceline/`):
- `sensor.hpp` — sensor sample types, fault injection interface, `Concept SensorSource`
- `fusion.hpp` — linear Kalman filter state estimator (constant-acceleration process model)
- `telemetry_bus.hpp` — lock-free single-producer/single-consumer ring buffer
- `flight_recorder.hpp` — append-only binary log with checksums, replayable
- `types.hpp` — shared fixed-size types, `std::expected`-based error handling

## 4. Key Engineering Decisions (and why)

| Decision | Rationale |
|---|---|
| `std::expected<T, Error>` instead of exceptions on the hot path | Exceptions have non-deterministic unwind cost; unacceptable in a bounded-latency loop. Documented explicitly — this is the kind of thing AI tools default to getting wrong (see ai-workflow.md). |
| Fixed-capacity containers (no `std::vector` growth in hot path) | Predictable memory footprint is a hard requirement in real-time systems. |
| Lock-free SPSC ring buffer for telemetry | Avoids priority inversion / lock contention between fusion thread and recorder thread. |
| C++23 concepts over template SFINAE | Readability and compiler error quality — matters more in a portfolio piece meant to be *read*, not just run. |
| clang-tidy MISRA-inspired ruleset in CI | Makes coding-standard discipline verifiable, not just claimed. |

## 5. Testing & Verification Strategy

- **Unit tests** (GoogleTest): per-module correctness, including fault-injection paths
- **Property-based tests**: fusion engine convergence properties (state estimate should converge given consistent sensor input, degrade gracefully given inconsistent input)
- **Benchmarks** (Google Benchmark): fusion cycle latency, telemetry bus throughput — numbers go in the README, not claims
- **Sanitizers**: ASan + UBSan on every CI run; TSan on the telemetry bus specifically (concurrency-sensitive)
- **Fuzzing**: libFuzzer harness on sensor sample deserialization (the flight recorder replay path is the most dangerous surface — untrusted-ish binary input)

## 6. Roadmap

| Phase | Scope |
|---|---|
| M1 | Repo scaffold, CI pipeline, requirements doc, sensor simulation layer — **done** |
| M2 | Fusion engine (constant-acceleration Kalman filter) + unit tests + benchmarks — **done**: real predict/update math, convergence-tested across multiple RNG seeds, ASan/UBSan clean |
| M3 | Telemetry bus + flight recorder, TSan-clean concurrency — **done**: multithreaded stress test (200k items) clean under TSan across repeated runs; verified the test itself catches races by deliberately breaking memory ordering; CRC32-checksummed flight recorder with corruption/truncation detection |
| M4 | Fault injection framework, property-based tests, fuzzing harness — **done**: `test_fusion.cpp` gained the `FaultInjectionTest_*` cases REQ-003 already claimed (dropout/out-of-range/stale-timestamp each verified not to crash the loop, plus a transient-fault-then-recovery case); RapidCheck-based property tests in `test_fusion_properties.cpp` check convergence and fault-survival across randomized inputs instead of one fixed seed/fault type; a libFuzzer harness (`fuzz/fuzz_flight_recorder.cpp`) fuzzes `FlightRecorder::decode_and_verify` directly, smoke-tested every CI run |
| M5 | Replay tool (CLI to visualize/replay recorded flights), polish docs |
| Ongoing | AI-workflow doc updated as patterns emerge; one external OSS contribution/month |

## 7. Known Limitations (stated up front, deliberately)

- Not certified, not intended to be — a demonstration of certification-adjacent *discipline*, not a certified artifact.
- Linear KF, not an Extended/Unscented KF or full sensor-fusion suite (v1 avoids nonlinear orientation estimation entirely, which is what would require an EKF/UKF) — scoped intentionally to keep the core loop provably deterministic.
- Simulated sensors only; no hardware-in-the-loop.
