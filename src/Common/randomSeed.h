#pragma once

#include <base/types.h>
#include <cstdint>

/** Returns a number suitable as seed for PRNG. Use clock_gettime, pid and so on. */
UInt64 randomSeed();
