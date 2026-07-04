# How AI Is Used in This Project

This doc is updated as the project progresses. Its purpose is to be specific and honest, not to claim "AI-powered development" as a buzzword.

## Where AI accelerates me

- **Boilerplate**: CMake target wiring, GoogleTest scaffolding, repetitive struct/serialization code
- **Test case generation**: given a function signature and its documented invariants, generating a first draft of edge-case tests, which I then review and extend — particularly useful for fault-injection scenarios I might not think to enumerate manually
- **Documentation drafts**: first-pass docstrings and README sections, which I edit for accuracy
- **Exploring alternatives**: asking for 2-3 ways to implement a lock-free ring buffer, then reasoning through the tradeoffs myself before picking one

## Where I override or don't trust AI output

- **Exception-based error handling suggestions**: AI assistants default to idiomatic "modern C++" patterns that often mean exceptions and `std::vector`/`std::string` freely. On the fusion hot path (REQ-002, REQ-006), this is wrong for the domain, and I've caught and reverted this pattern multiple times — logged in commit history, not hidden.
- **Concurrency code**: AI-generated lock-free code is a starting sketch, never a final answer. Every concurrent data structure in this repo is hand-verified against the C++ memory model and run under TSan before I trust it, regardless of how confident the suggestion looked.
- **Timing/determinism claims**: AI cannot benchmark on my hardware. Every latency claim in this repo comes from `benchmarks/`, not from AI-generated comments asserting performance.
- **Silent scope creep**: AI tools tend to over-implement — adding retry logic, config options, or abstractions I didn't ask for. I actively prune this; the requirements doc is the contract, not the AI's judgment of what's "nice to have."

## Representative example (updated per module)

> Module: `fusion.cpp` (M2 -- the fusion engine implementation)
> Used Claude to draft the fixed-size linear algebra helpers (`linalg.hpp`) and the constant-acceleration Kalman filter predict/update math. Caught during review, not generated correctly by default:
> 1. Initial code targeted C++20, but `std::expected` is C++23 -- a real version mixup, fixed by bumping the standard and re-auditing every doc reference to it.
> 2. A convergence test initially asserted too tight a bound on a single noisy sample instead of averaging over the filter's steady state -- the first version was a flaky test, not a filter bug. Fixed by widening the averaging window and validating across multiple RNG seeds before trusting the bound.
> 3. Found (independently of AI, via manually running clang-tidy against the real repo) a genuine Clang 18 / libstdc++ interop gap: Clang's `__cpp_concepts` macro under-reports its value, which silently disables `std::expected` when compiled with clang-tidy. Documented and worked around in `ci.yml` rather than papered over.
>
> The pattern worth naming: AI-assisted code that "looks done" (compiles, has tests) still needs the same verification discipline as hand-written code -- run it under sanitizers, check it against multiple random seeds, run the actual linter, not just trust that it's correct because it was generated confidently.

> Module: `flight_recorder.cpp` (M3)
> Implemented CRC32-checksummed binary logging and replay. A real bug surfaced during testing, not code review: the constructor opened the output file stream eagerly in append mode, which meant constructing a `FlightRecorder` purely to *read* a log via `replay()` silently created an empty file if one didn't exist yet. Caught because `ReplayOnMissingFileReturnsError` failed when run, not because anyone spotted it reading the code. Fixed by opening the write stream lazily on the first `record()` call instead of in the constructor. Left in as the real example it is, including the failing test output that found it.

> Module: `telemetry_bus.cpp` (M3)
> Wrote a genuine multithreaded stress test (`ConcurrentPushPopUnderStress`, 200k items through producer/consumer threads) and ran it under ThreadSanitizer -- clean across 5 consecutive runs. To confirm the test could actually catch a real bug rather than passing vacuously, I deliberately weakened the acquire/release ordering to relaxed and reran: TSan correctly flagged a data race at the exact line the ordering was weakened, and the process exited nonzero (confirmed explicitly, since a race being *reported* doesn't automatically fail a test unless the exit code is checked in CI). Ordering restored afterward. This is the verification discipline REQ-004 is actually for -- not asserting correctness in a comment, but testing that the test itself is capable of catching the failure mode it claims to guard against.

This section will be filled in with real commit references as each module lands — the goal is that anyone reviewing this repo can see the actual review trail, not a claim about one.
