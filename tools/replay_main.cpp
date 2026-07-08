#include "replay_format.hpp"
#include "traceline/flight_recorder.hpp"
#include <cstdio>
#include <string_view>

namespace {

void print_usage(const char* prog) {
    std::fprintf(stderr, "usage: %s <path-to-log> [--csv]\n", prog);
}

// Local to the CLI -- the core library only needs Error values to compare
// and propagate (std::expected), not to print, so no to_string lives in
// types.hpp.
const char* error_name(traceline::Error err) {
    switch (err) {
        case traceline::Error::SensorTimeout:
            return "SensorTimeout";
        case traceline::Error::SensorOutOfRange:
            return "SensorOutOfRange";
        case traceline::Error::StaleTimestamp:
            return "StaleTimestamp";
        case traceline::Error::BufferFull:
            return "BufferFull";
        case traceline::Error::BufferEmpty:
            return "BufferEmpty";
        case traceline::Error::ChecksumMismatch:
            return "ChecksumMismatch";
        case traceline::Error::NotInitialized:
            return "NotInitialized";
        case traceline::Error::SingularMatrix:
            return "SingularMatrix";
    }
    return "Unknown";
}

}  // namespace

int main(int argc, char** argv) {
    std::string_view path;
    bool csv = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--csv") {
            csv = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (path.empty()) {
            path = arg;
        } else {
            std::fprintf(stderr, "unexpected argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    if (path.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    if (csv) {
        std::printf("%s\n", traceline::tools::csv_header().c_str());
    }

    std::size_t count = 0;
    traceline::FlightRecorder recorder(path);
    auto result = recorder.replay([&](const traceline::FlightRecorder::Entry& entry) {
        const std::string line = csv ? traceline::tools::format_entry_csv(entry, count)
                                      : traceline::tools::format_entry_text(entry, count);
        std::printf("%s\n", line.c_str());
        ++count;
    });

    if (!result.has_value()) {
        std::fprintf(stderr, "replay stopped after %zu valid record(s): %s\n", count,
                      error_name(result.error()));
        return 1;
    }

    std::fprintf(stderr, "%zu record(s) replayed successfully.\n", count);
    return 0;
}
