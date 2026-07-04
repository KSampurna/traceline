#include "traceline/flight_recorder.hpp"
#include <array>
#include <cstring>

namespace traceline {

namespace {

// Standard CRC32 (IEEE 802.3 polynomial, 0xEDB88320), table-based.
// Table is computed once at first use -- deliberately not constexpr
// (a 256-entry constexpr table is fine too, but a lazily-built static
// table keeps this translation unit simple and the cost is paid once
// per process, not per record).
const std::array<std::uint32_t, 256>& crc32_table() {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            }
            t[i] = c;
        }
        return t;
    }();
    return table;
}

}  // namespace

std::uint32_t FlightRecorder::crc32(const unsigned char* data, std::size_t len) noexcept {
    const auto& table = crc32_table();
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

Result<void> FlightRecorder::record(const StateEstimate& estimate) noexcept {
    if (!out_.is_open()) {
        out_.open(path_, std::ios::binary | std::ios::app);
        if (!out_.is_open()) {
            return std::unexpected(Error::NotInitialized);
        }
    }

    const std::int64_t ts_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(estimate.timestamp.time_since_epoch())
            .count();

    std::array<char, kRecordSize> buf{};
    std::memcpy(buf.data(), &ts_ns, sizeof(ts_ns));
    std::memcpy(buf.data() + sizeof(ts_ns), estimate.state.data(), sizeof(double) * 9);

    const auto payload_len = sizeof(ts_ns) + (sizeof(double) * 9);
    const std::uint32_t crc =
        crc32(reinterpret_cast<const unsigned char*>(buf.data()), payload_len);
    std::memcpy(buf.data() + payload_len, &crc, sizeof(crc));

    out_.write(buf.data(), static_cast<std::streamsize>(kRecordSize));
    if (!out_) {
        return std::unexpected(Error::BufferFull);  // disk write failure
    }
    out_.flush();
    return {};
}

Result<FlightRecorder::Entry> FlightRecorder::decode_and_verify(
    const std::array<char, kRecordSize>& buf) noexcept {
    const auto payload_len = sizeof(std::int64_t) + (sizeof(double) * 9);

    std::uint32_t stored_crc = 0;
    std::memcpy(&stored_crc, buf.data() + payload_len, sizeof(stored_crc));
    const std::uint32_t actual_crc =
        crc32(reinterpret_cast<const unsigned char*>(buf.data()), payload_len);
    if (stored_crc != actual_crc) {
        return std::unexpected(Error::ChecksumMismatch);
    }

    Entry entry{};
    std::memcpy(&entry.timestamp_ns, buf.data(), sizeof(entry.timestamp_ns));
    std::memcpy(entry.state.data(), buf.data() + sizeof(entry.timestamp_ns), sizeof(double) * 9);
    return entry;
}

}  // namespace traceline
