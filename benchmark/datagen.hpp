#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace datagen {

using u64 = std::uint64_t;
using i64 = std::int64_t;

enum class SignedMode {
    NonNegative,
    Signed
};

enum class Pattern {
    Random,
    Runs,
    AlmostSorted
};

inline const char* to_string(SignedMode m) {

    return (m == SignedMode::Signed) ? "signed" : "nonneg";
}
inline const char* to_string(Pattern p) {
    switch (p) {
        case Pattern::Random: return "random";
        case Pattern::Runs: return "runs";
        case Pattern::AlmostSorted: return "almost_sorted";
    }
    return "random";
}

inline u64 mix_u64(u64 x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

inline std::uint32_t effective_entropy(std::size_t count, std::uint32_t requested) {
    if (count == 0) return 0;
    std::uint64_t eff = requested;
    if (eff < 1) eff = 1;
    if (eff > count) eff = static_cast<std::uint64_t>(count);

    if (eff > std::numeric_limits<std::uint32_t>::max()) eff = std::numeric_limits<std::uint32_t>::max();
    return static_cast<std::uint32_t>(eff);
}

inline std::vector<u64> build_palette(std::uint32_t k_eff, u64 seed) {
    if (k_eff == 0) return {0};

    const u64 start = mix_u64(seed ^ 0xA5A5A5A5A5A5A5A5ULL);
    const u64 step  = (mix_u64(seed ^ 0xC3C3C3C3C3C3C3C3ULL) | 1ULL);

    std::vector<u64> palette;
    palette.reserve(k_eff);
    for (std::uint32_t i = 0; i < k_eff; ++i) {
        palette.push_back(start + step * static_cast<u64>(i));
    }
    return palette;
}

inline std::vector<u64> uniform(std::size_t count, u64  = 0, u64 seed = 42) {
    std::mt19937_64 rng(seed);
    std::vector<u64> data(count);
    std::generate(data.begin(), data.end(), [&]() { return static_cast<u64>(rng()); });
    return data;
}

inline std::vector<u64> low_cardinality(std::size_t count, std::size_t unique_values, u64 seed = 42) {
    if (unique_values == 0) unique_values = 1;
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<u64> dist(0, static_cast<u64>(unique_values - 1));
    std::vector<u64> data(count);
    std::generate(data.begin(), data.end(), [&]() { return dist(rng); });
    return data;
}

inline std::vector<u64> almost_sorted(std::size_t count, std::size_t swaps, u64 seed = 42) {
    std::vector<u64> data(count);
    for (std::size_t i = 0; i < count; ++i) data[i] = static_cast<u64>(i);
    if (count < 2) return data;

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<std::size_t> dist(0, count - 1);
    for (std::size_t i = 0; i < swaps; ++i) {
        std::size_t a = dist(rng);
        std::size_t b = dist(rng);
        std::swap(data[a], data[b]);
    }
    return data;
}

struct LowEntropyResult {
    std::vector<u64> data;
    std::uint32_t entropy_eff = 0;
};

inline LowEntropyResult low_entropy(std::size_t count,
                                   i64 ,
                                   std::uint32_t entropy_requested,
                                   SignedMode ,
                                   Pattern pattern,
                                   u64 seed,
                                   std::size_t swaps_for_almost_sorted = 0) {
    if (count == 0) return { {}, 0 };

    const std::uint32_t k_eff = effective_entropy(count, entropy_requested);
    auto palette = build_palette(k_eff, seed);

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<std::size_t> pick(0, palette.size() - 1);

    std::vector<u64> data(count);

    if (pattern == Pattern::Random) {
        for (std::size_t i = 0; i < count; ++i) data[i] = palette[pick(rng)];
    } else if (pattern == Pattern::Runs) {
        std::uniform_int_distribution<int> runlen_dist(1, 512);
        std::size_t i = 0;
        while (i < count) {
            u64 v = palette[pick(rng)];
            int runlen = runlen_dist(rng);
            std::size_t end = std::min<std::size_t>(count, i + static_cast<std::size_t>(runlen));
            for (; i < end; ++i) data[i] = v;
        }
    } else {
        for (std::size_t i = 0; i < count; ++i) data[i] = palette[pick(rng)];
        std::sort(data.begin(), data.end());
        if (count >= 2) {
            if (swaps_for_almost_sorted == 0) {
                swaps_for_almost_sorted = std::max<std::size_t>(32, count / 100);
            }
            std::uniform_int_distribution<std::size_t> dist(0, count - 1);
            for (std::size_t s = 0; s < swaps_for_almost_sorted; ++s) {
                std::size_t a = dist(rng);
                std::size_t b = dist(rng);
                std::swap(data[a], data[b]);
            }
        }
    }

    return { std::move(data), static_cast<std::uint32_t>(palette.size()) };
}

}
