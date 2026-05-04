#pragma once

#include <random>

namespace vt {

// Thread-local RNG — safe for use in OpenMP parallel regions.
// Each thread gets its own independently-seeded Mersenne Twister.
inline thread_local std::mt19937_64 t_rng{std::random_device{}()};

inline float randomFloat() {
    static thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(t_rng);
}

} // namespace vt
