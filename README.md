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

## Using Claude Code / MCP on this repo

This repo ships a project-scoped `.mcp.json` that wires up the GitHub MCP server for Claude Code, scoped to this project only.

**Note:** Claude Code does not automatically load `.env` files into the environment used for `.mcp.json` variable expansion (this is a known gap, not yet built in as of writing). The token needs to be an actual exported shell environment variable before you launch Claude Code:

```bash
export GITHUB_PERSONAL_ACCESS_TOKEN=your-token-here
claude
```

`.env.example` in this repo documents the expected variable name for reference; it's not auto-loaded. If you use `direnv` or a similar tool with a personal `.envrc`, that's a reasonable way to avoid re-exporting it every session — just don't commit that file either (it's gitignored alongside `.env`).

A fine-grained PAT limited to just this repo (rather than a classic broad-scope token) is the safer choice.

**If `${GITHUB_PERSONAL_ACCESS_TOKEN}` doesn't expand correctly** in the `headers` block above (there have been Claude Code versions with header-substitution bugs), fall back to registering the server via the CLI instead of the static `.mcp.json`:
```bash
claude mcp add --transport http github https://api.githubcopilot.com/mcp -H "Authorization: Bearer $GITHUB_PERSONAL_ACCESS_TOKEN"
```
Note this writes the *resolved* token into your local `.mcp.json` (a known quirk, not just this repo's issue) — double-check `git diff .mcp.json` before ever committing after using this method, so a real token doesn't end up in history.

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
