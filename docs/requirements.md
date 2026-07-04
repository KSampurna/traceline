# Requirements & Traceability

Lightweight, portfolio-scale version of requirements-to-code-to-test traceability, inspired by (not claiming compliance with) DO-178C-style practices. Each requirement has a stable ID, referenced in the implementing code via a comment tag `// @req REQ-XXX` and in the corresponding test via `// @verifies REQ-XXX`.

| ID | Requirement | Implemented in | Verified by |
|---|---|---|---|
| REQ-001 | Fusion cycle shall complete in bounded time with p99.9 < 500µs | `fusion.cpp` | `bench_fusion.cpp` |
| REQ-002 | Fusion engine shall not allocate on the heap after initialization | `fusion.cpp` | `test_fusion.cpp::NoHeapAllocationTest` |
| REQ-003 | Sensor faults (dropout, out-of-range, stale timestamp) shall not crash the fusion loop | `sensor.hpp`, `fusion.cpp` | `test_fusion.cpp::FaultInjectionTest` |
| REQ-004 | Telemetry bus shall be safe for exactly one producer and one consumer thread with no locks | `telemetry_bus.cpp` | `test_telemetry_bus.cpp` (TSan-clean CI job) |
| REQ-005 | Flight recorder entries shall be checksummed and detect corruption on replay | `flight_recorder.cpp` | `test_flight_recorder.cpp::CorruptionDetectionTest` |
| REQ-006 | All public error paths shall use `std::expected`, not exceptions | all `include/traceline/*.hpp` | code review + `-fno-exceptions` build target in CI |

This table is intentionally small at project start — it grows as modules are built (M2 onward). The point isn't exhaustiveness on day one; it's that every future PR either adds a requirement row or references an existing one. That discipline, visible in PR history, is the actual signal for reviewers coming from safety-critical backgrounds.
