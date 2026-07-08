#include "traceline/fusion.hpp"
#include <benchmark/benchmark.h>

// @req REQ-001: this benchmark is the source of truth for the p99.9 <
// 500us latency claim in docs/design.md. There is no CI benchmark-gate
// job enforcing this yet -- numbers here are measured, not asserted;
// see README.md's Performance section and docs/ai-workflow.md.
static void BM_FusionPredict(benchmark::State& state) {
    traceline::FusionEngine engine;
    for (auto _ : state) {
        auto result = engine.predict(std::chrono::steady_clock::now());
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FusionPredict);
