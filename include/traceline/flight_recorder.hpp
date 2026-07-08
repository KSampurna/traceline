#pragma once

#include "traceline/fusion.hpp"
#include "traceline/types.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>

namespace traceline {

// On-disk record layout (fixed size, binary, little-endian on all
// platforms this targets):
//   [8 bytes]  timestamp, nanoseconds since steady_clock epoch (int64_t)
//   [72 bytes] 9 doubles: the state vector (position/velocity/acceleration)
//   [4 bytes]  CRC32 of the 80 bytes above
// Total: 84 bytes/record.
//
// Deliberately NOT logging the full 9x9 covariance matrix (648 bytes)
// per entry -- for a black-box flight recorder, the state estimate is
// what an investigator or replay tool needs; covariance is filter
// internals, not flight history. This is a scoped decision, not an
// oversight -- documented here so it's a visible tradeoff, not a
// silent one.
inline constexpr std::size_t kRecordSize = 8 + (9 * 8) + 4;

// @req REQ-005: append-only log of state estimates, each with a CRC32
// checksum so replay can detect corruption rather than silently
// trusting a damaged log -- the "black box" of this system.
class FlightRecorder {
public:
    // A single decoded, checksum-verified log entry.
    struct Entry {
        std::int64_t timestamp_ns = 0;
        std::array<double, 9> state{};
    };

    explicit FlightRecorder(std::string_view path) : path_(path) {}
    // NOTE: the write stream is intentionally NOT opened here. An
    // earlier version opened it eagerly in the constructor, which
    // meant simply constructing a FlightRecorder to *read* a log (via
    // replay()) silently created an empty file as a side effect --
    // caught by ReplayOnMissingFileReturnsError failing unexpectedly
    // during real testing, not by inspection. The stream now opens
    // lazily on the first record() call instead.

    Result<void> record(const StateEstimate& estimate) noexcept;

    // Replays the log file from the beginning, calling `visitor(entry)`
    // for each valid record in order. Stops and returns an error on
    // the first checksum mismatch or truncated/short record -- a
    // partially-corrupted log yields a partial, trustworthy replay
    // rather than either silently skipping bad data or crashing.
    template <typename Visitor>
    Result<void> replay(Visitor&& visitor) const {
        std::ifstream in(path_, std::ios::binary);
        if (!in) {
            return std::unexpected(Error::NotInitialized);
        }

        std::array<char, kRecordSize> buf{};
        while (in.read(buf.data(), static_cast<std::streamsize>(kRecordSize))) {
            auto decoded = decode_and_verify(buf);
            if (!decoded.has_value()) {
                return std::unexpected(decoded.error());
            }
            visitor(*decoded);
        }
        // A short/partial trailing read (in.gcount() > 0 but < kRecordSize)
        // means the file was truncated mid-record -- e.g. a crash during
        // write. Treat as corruption, not silent truncation.
        if (in.eof() && in.gcount() > 0 &&
            static_cast<std::size_t>(in.gcount()) < kRecordSize) {
            return std::unexpected(Error::ChecksumMismatch);
        }
        return {};
    }

    // Decodes and checksum-verifies a single raw record buffer. Public
    // (rather than an implementation detail of replay()) because it's a
    // pure function of its argument -- no private state -- and is the
    // direct entry point for fuzzing the flight-recorder binary format
    // (see fuzz/fuzz_flight_recorder.cpp): fuzzing through replay()'s file
    // I/O would mean a disk write per iteration, which is far slower than
    // calling the decoder directly on fuzzer-provided bytes.
    static Result<Entry> decode_and_verify(const std::array<char, kRecordSize>& buf) noexcept;

private:
    static std::uint32_t crc32(const unsigned char* data, std::size_t len) noexcept;

    std::string path_;
    std::ofstream out_;
};

}  // namespace traceline
