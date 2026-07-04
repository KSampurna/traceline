# Traceline

A deterministic multi-sensor fusion and flight-data recorder engine, built in modern C++23 with the discipline (not the certification) of safety-critical/avionics software practice.

**Status:** early scaffold (M1 in progress) — see [roadmap](docs/design.md#6-roadmap).

## Why this exists

Most portfolio C++ projects optimize for "looks impressive in a demo." This one optimizes for the constraints that actually matter in aviation, defence, and reliability-critical enterprise systems: bounded latency, no unbounded memory growth, requirements traceability, and an honest account of where AI tooling helps versus where it needs to be overruled. Background: I came to this from aviation/defence and enterprise engineering, not from a CS-degree-to-startup pipeline, and this project is built the way I'd actually want a safety-adjacent codebase built.

## Who this is for (and isn't)

This will not run in an airline's fleet, and it isn't trying to. Certified avionics software goes through DO-178C qualification and hazard analysis this project doesn't attempt — see [Known Limitations](#whats-not-here-yet-honest-limitations) below.

Where this is actually relevant: **avionics/aerospace suppliers** (the companies that build what airlines fly — Honeywell, Collins Aerospace, Thales, GE Aerospace, and smaller Tier 2/3 suppliers), **flight data monitoring / FOQA tooling**, and **predictive maintenance** vendors, all of which need engineers who can build deterministic sensor fusion with real verification discipline, without necessarily needing full DO-178C certification experience on day one. This project is built to demonstrate that discipline directly, rather than asking someone to take it on faith from a resume line about NDA'd work.

## Architecture

See [docs/design.md](docs/design.md) for the full design doc, including the reasoning behind each major decision (why `std::expected` over exceptions, why lock-free over locked, etc.)

```
Sensors (simulated, fault-injectable) → Fusion Engine (linear Kalman filter, constant-acceleration model) → Telemetry Bus (lock-free SPSC) → Flight Recorder
```

## Requirements traceability

Every module ties back to a numbered requirement, referenced in code (`@req REQ-XXX`) and tests (`@verifies REQ-XXX`). See [docs/requirements.md](docs/requirements.md).

## How AI was used to build this

Documented honestly, including where AI-generated suggestions were wrong for this domain and had to be overridden. See [docs/ai-workflow.md](docs/ai-workflow.md).

## Performance

Benchmarks exist in `benchmarks/` from day one so every performance claim is measured, not asserted. Run `traceline_benchmarks` locally for current numbers (CI-published numbers land once M3's benchmark-gate job is added).

## Building

```bash
cmake --preset debug        # ASan+UBSan debug build
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure

# Or release:
cmake --preset release
cmake --build --preset release

# Or enforce the no-exceptions hot-path contract:
cmake --preset no-exceptions
cmake --build --preset no-exceptions
```

Requires a C++23 compiler (GCC 12+/Clang 15+), CMake 3.25+.

## What's not here yet (honest limitations)

- Flight recorder persistence (scaffolded — M3)
- Fault injection framework, fuzzing harness (M4)
- Nonlinear orientation estimation (would require an EKF/UKF — out of scope for v1, see [design doc Non-Goals](docs/design.md#2-goals--non-goals))
- Not certified to DO-178C or any standard — this borrows the *discipline* of that world, and says so explicitly rather than implying otherwise.

## License

MIT — see [LICENSE](LICENSE).
