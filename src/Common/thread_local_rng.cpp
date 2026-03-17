#include <Common/randomSeed.h>
#include <Common/thread_local_rng.h>

thread_local pcg64 thread_local_rng{randomSeed()};
