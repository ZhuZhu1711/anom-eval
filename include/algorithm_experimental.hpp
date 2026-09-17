#pragma once

#include "dd_types.hpp"

// Experimental group: same pipeline as control, with the horizontal leftover
// pass rewritten so a pack is recorded when it same-channel-connects across
// a 4-pack boundary (to a cluster, leftover double, or leftover single).
// Isolated singles and isolated adjacent doubles still pass (DPC).
DetectResult run_experimental(const Image& src);
