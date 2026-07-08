#include "traceline/flight_recorder.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

// Fuzzes FlightRecorder::decode_and_verify directly with libFuzzer-provided
// bytes -- the flight-recorder replay path is the most dangerous surface in
// this codebase (untrusted-ish binary input; see docs/design.md section 5).
// decode_and_verify is noexcept and operates only on its buffer argument,
// so this harness needs no setup beyond padding/truncating the fuzzer's
// bytes into a fixed kRecordSize buffer.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    std::array<char, traceline::kRecordSize> buf{};
    const std::size_t copy_len = std::min(size, buf.size());
    std::copy_n(data, copy_len, buf.data());

    // The property under test is simply that this call never crashes and
    // always returns a well-defined Result -- ASan/the sanitizer runtime
    // is what actually flags a failure, not an assertion here.
    auto result = traceline::FlightRecorder::decode_and_verify(buf);
    (void)result;
    return 0;
}
