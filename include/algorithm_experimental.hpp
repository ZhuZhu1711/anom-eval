#pragma once

#include "dd_types.hpp"

// Experimental group: same pipeline as control, with the horizontal leftover
// pass rewritten so singles / adjacent-doubles that connect to a neighboring
// cluster are recorded as a more complex pattern. Isolated singles and
// isolated adjacent doubles still pass (DPC).
DetectResult run_experimental(const Image& src);
