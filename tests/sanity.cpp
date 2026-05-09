#include "cafs2.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

static bool run_case(const char* label, std::size_t N, std::size_t K,
                     std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<std::uint64_t> palette(K);
    for (std::size_t i = 0; i < K; ++i) palette[i] = rng();

    std::uniform_int_distribution<std::size_t> pick(0, K - 1);
    std::vector<std::uint64_t> data(N);
    for (std::size_t i = 0; i < N; ++i) data[i] = palette[pick(rng)];

    std::vector<std::uint64_t> ref = data;
    std::sort(ref.begin(), ref.end());

    cafs2::cafs_sort(data);

    bool ok = (data == ref);
    std::printf("  %-7s N=%-9zu K=%-9zu : %s\n",
                label, N, K, ok ? "ok" : "MISMATCH");
    return ok;
}

int main() {
    constexpr std::size_t N = 1'000'000;

    bool all_ok = true;
    all_ok &= run_case("tiny",   N, 4,         100);
    all_ok &= run_case("main",   N, 1'000,     101);
    all_ok &= run_case("highK",  N, 100'000,   102);

    std::printf("%s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}
