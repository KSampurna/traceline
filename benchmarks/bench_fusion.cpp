#include "traceline/fusion.hpp"
#include <benchmark/benchmark.h>

// @req REQ-001: this benchmark is the source of truth for the p99.9 <
// 500us latency claim in docs/design.md -- once the real EKF math
// lands in M2, CI will fail the build if this regresses past target
// (see .github/workflows/ci.yml benchmark-gate job, added in M2).
static void BM_FusionPredict(benchmark::State& state) {
    traceline::FusionEngine engine;
    for (auto _ : state) {
        auto result = engine.predict(std::chrono::steady_clock::now());
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FusionPredict);
