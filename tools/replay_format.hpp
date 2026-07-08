#pragma once

#include "traceline/flight_recorder.hpp"
#include <cstddef>
#include <string>

namespace traceline::tools {

// Human-readable one-line-per-entry rendering: index, timestamp, and the
// 9-element state vector (position/velocity/acceleration per axis -- see
// StateEstimate's layout comment in fusion.hpp).
std::string format_entry_text(const FlightRecorder::Entry& entry, std::size_t index);

// CSV rendering of the same fields, for piping into external
// plotting/analysis tools -- this project deliberately doesn't pull in a
// plotting library (see linalg.hpp's rationale for avoiding heavy
// dependencies in favor of hand-rolled, purpose-fit code).
std::string format_entry_csv(const FlightRecorder::Entry& entry, std::size_t index);

// CSV header line matching format_entry_csv's column order.
std::string csv_header();

}  // namespace traceline::tools
