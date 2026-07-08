#include "replay_format.hpp"
#include <sstream>

namespace traceline::tools {

std::string format_entry_text(const FlightRecorder::Entry& entry, std::size_t index) {
    std::ostringstream out;
    out << '[' << index << "] t=" << entry.timestamp_ns << "ns"
        << "  pos=(" << entry.state[0] << ", " << entry.state[1] << ", " << entry.state[2] << ')'
        << "  vel=(" << entry.state[3] << ", " << entry.state[4] << ", " << entry.state[5] << ')'
        << "  accel=(" << entry.state[6] << ", " << entry.state[7] << ", " << entry.state[8]
        << ')';
    return out.str();
}

std::string csv_header() {
    return "index,timestamp_ns,px,py,pz,vx,vy,vz,ax,ay,az";
}

std::string format_entry_csv(const FlightRecorder::Entry& entry, std::size_t index) {
    std::ostringstream out;
    out << index << ',' << entry.timestamp_ns;
    for (double v : entry.state) {
        out << ',' << v;
    }
    return out.str();
}

}  // namespace traceline::tools
