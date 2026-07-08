#include "traceline/sensor.hpp"

// Implementation intentionally minimal -- SimulatedImu is fully defined
// inline in the header for now. Additional sensor types (GPS, barometric
// altimeter) aren't part of the M1-M5 roadmap (see docs/design.md); if
// pursued later, their noise models would land here.
