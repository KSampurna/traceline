#include "traceline/telemetry_bus.hpp"
#include <benchmark/benchmark.h>

static void BM_TelemetryBusPushPop(benchmark::State& state) {
    traceline::TelemetryBus<int, 1024> bus;
    int value = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(bus.push(value));
        benchmark::DoNotOptimize(bus.pop());
        ++value;
    }
}
BENCHMARK(BM_TelemetryBusPushPop);
