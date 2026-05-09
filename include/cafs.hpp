#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <vector>
#include <type_traits>
#include <limits>
#include <immintrin.h>
#include <cstring>
#include <ranges>
#include <cstdlib>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <malloc.h>
#endif

namespace cafs {

struct CafsParams {
    std::size_t total_slots = 16384;

    double spill_ratio_limit = 0.3;
};

namespace detail {

inline void* aligned_alloc_wrapper(std::size_t alignment, std::size_t size) {
#if defined(_WIN32) || defined(__CYGWIN__)
    return _aligned_malloc(size, alignment);
#else

    return std::aligned_alloc(alignment, size);
#endif
}

inline void aligned_free_wrapper(void* ptr) {
#if defined(_WIN32) || defined(__CYGWIN__)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

template <typename T>
[[nodiscard]] inline int check_order(const T* data, size_t n) {
    if (n < 2) return 1;

    size_t i = 0;

    while (i + 1 < n && data[i] == data[i+1]) ++i;
    if (i + 1 == n) return 1;

    if (data[i] < data[i+1]) {

        for (; i + 1 < n; ++i) {
            if (data[i] > data[i+1]) [[unlikely]] return 0;
        }
        return 1;
    } else {

        for (; i + 1 < n; ++i) {
            if (data[i] < data[i+1]) [[unlikely]] return 0;
        }
        return -1;
    }
}

template <typename T>
struct Bucket {
    static constexpr int CAPACITY = 4;
    T keys[CAPACITY];
    std::uint32_t counts[CAPACITY];

    inline bool update(T val, std::uint32_t count) {
        for (int i = 0; i < CAPACITY; ++i) {
            if (counts[i] > 0) {
                if (keys[i] == val) {
                    counts[i] += count;
                    return true;
                }
            } else {
                keys[i] = val;
                counts[i] = count;
                return true;
            }
        }
        return false;
    }
};

template <>
struct alignas(64) Bucket<std::int32_t> {
    static constexpr int CAPACITY = 8;
    std::int32_t keys[CAPACITY];
    std::uint32_t counts[CAPACITY];

    [[gnu::always_inline]]
    inline bool update(std::int32_t val, std::uint32_t inc_cnt) {

        __m256i v_keys = _mm256_load_si256(reinterpret_cast<const __m256i*>(keys));
        __m256i v_val = _mm256_set1_epi32(val);
        __m256i cmp = _mm256_cmpeq_epi32(v_keys, v_val);
        int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));

        if (mask != 0) [[likely]] {
            int idx = std::countr_zero(static_cast<unsigned int>(mask));
            counts[idx] += inc_cnt;
            return true;
        }

        __m256i v_counts = _mm256_load_si256(reinterpret_cast<const __m256i*>(counts));
        __m256i v_zero = _mm256_setzero_si256();
        __m256i cmp_zero = _mm256_cmpeq_epi32(v_counts, v_zero);
        int free_mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp_zero));

        if (free_mask != 0) [[likely]] {
            int idx = std::countr_zero(static_cast<unsigned int>(free_mask));
            keys[idx] = val;
            counts[idx] = inc_cnt;
            return true;
        }

        return false;
    }
};

template <>
struct alignas(64) Bucket<std::uint64_t> {
    static constexpr int CAPACITY = 4;
    std::uint64_t keys[CAPACITY];
    std::uint32_t counts[CAPACITY];
    std::uint32_t _pad[CAPACITY];

    [[gnu::always_inline]]
    inline bool update(std::uint64_t val, std::uint32_t inc_cnt) {
        __m256i v_keys = _mm256_load_si256(reinterpret_cast<const __m256i*>(keys));
        __m256i v_val  = _mm256_set1_epi64x(static_cast<long long>(val));
        __m256i cmp    = _mm256_cmpeq_epi64(v_keys, v_val);
        int mask = _mm256_movemask_pd(_mm256_castsi256_pd(cmp));

        if (mask != 0) [[likely]] {
            int idx = std::countr_zero(static_cast<unsigned int>(mask));
            counts[idx] += inc_cnt;
            return true;
        }

        if (counts[0] == 0) { keys[0] = val; counts[0] = inc_cnt; return true; }
        if (counts[1] == 0) { keys[1] = val; counts[1] = inc_cnt; return true; }
        if (counts[2] == 0) { keys[2] = val; counts[2] = inc_cnt; return true; }
        if (counts[3] == 0) { keys[3] = val; counts[3] = inc_cnt; return true; }

        return false;
    }
};

template <>
struct alignas(64) Bucket<std::int64_t> {
    static constexpr int CAPACITY = 4;
    std::int64_t keys[CAPACITY];
    std::uint32_t counts[CAPACITY];
    std::uint32_t _pad[CAPACITY];

    [[gnu::always_inline]]
    inline bool update(std::int64_t val, std::uint32_t inc_cnt) {
        __m256i v_keys = _mm256_load_si256(reinterpret_cast<const __m256i*>(keys));
        __m256i v_val = _mm256_set1_epi64x(val);
        __m256i cmp = _mm256_cmpeq_epi64(v_keys, v_val);
        int mask = _mm256_movemask_pd(_mm256_castsi256_pd(cmp));

        if (mask != 0) [[likely]] {
            int idx = std::countr_zero(static_cast<unsigned int>(mask));
            counts[idx] += inc_cnt;
            return true;
        }

        if (counts[0] == 0) { keys[0] = val; counts[0] = inc_cnt; return true; }
        if (counts[1] == 0) { keys[1] = val; counts[1] = inc_cnt; return true; }
        if (counts[2] == 0) { keys[2] = val; counts[2] = inc_cnt; return true; }
        if (counts[3] == 0) { keys[3] = val; counts[3] = inc_cnt; return true; }

        return false;
    }
};

inline std::size_t fast_hash(std::uint64_t val, int shift) {
    return (val * 11400714819323198485ULL) >> shift;
}

template <typename T>
inline std::uint64_t as_u64(T v) {
    if constexpr (sizeof(T) == 4) return static_cast<std::uint64_t>(static_cast<std::uint32_t>(v));
    return static_cast<std::uint64_t>(v);
}

}

template <typename T>
void cafs_sort(std::vector<T>& data, CafsParams params = CafsParams{}) {
    using namespace detail;
    const std::size_t N = data.size();
    if (N < 2) return;

    int order = check_order(data.data(), N);
    if (order == 1) return;
    if (order == -1) {
        std::reverse(data.begin(), data.end());
        return;
    }

    std::size_t req_slots = std::max(params.total_slots, N >> 1);
    std::size_t num_buckets = std::bit_ceil(req_slots) / Bucket<T>::CAPACITY;
    if (num_buckets < 4) num_buckets = 4;

    const std::size_t bucket_mask = num_buckets - 1;
    const int hash_shift = 64 - std::bit_width(bucket_mask);

    auto* table = static_cast<Bucket<T>*>(
        detail::aligned_alloc_wrapper(64, num_buckets * sizeof(Bucket<T>))
    );

    if (!table) return;

    std::memset(table, 0, num_buckets * sizeof(Bucket<T>));

    std::vector<T> spill;
    spill.reserve(N >> 5);

    const T* ptr = data.data();
    size_t i = 0;

    while (i < N) {
        T val = ptr[i];

        size_t run_len = 1;
        while (i + run_len < N && ptr[i + run_len] == val) {
            run_len++;
        }

        size_t idx = fast_hash(as_u64(val), hash_shift) & bucket_mask;

        if (!table[idx].update(val, static_cast<std::uint32_t>(run_len))) [[unlikely]] {

            for(size_t k=0; k<run_len; ++k) spill.push_back(val);
        }

        i += run_len;
    }

    std::vector<std::pair<T, std::uint32_t>> dense_res;
    dense_res.reserve(num_buckets);

    for (size_t b = 0; b < num_buckets; ++b) {
        const auto& bucket = table[b];
        for (int s = 0; s < Bucket<T>::CAPACITY; ++s) {
            if (bucket.counts[s] > 0) {
                dense_res.emplace_back(bucket.keys[s], bucket.counts[s]);
            }
        }
    }

    detail::aligned_free_wrapper(table);

    if (!spill.empty()) {
        std::sort(spill.begin(), spill.end());
        T curr = spill[0];
        uint32_t cnt = 1;
        for (size_t k = 1; k < spill.size(); ++k) {
            if (spill[k] == curr) cnt++;
            else {
                dense_res.emplace_back(curr, cnt);
                curr = spill[k];
                cnt = 1;
            }
        }
        dense_res.emplace_back(curr, cnt);
    }

    std::sort(dense_res.begin(), dense_res.end(),
        [](const auto& a, const auto& b){ return a.first < b.first; });

    auto out_it = data.begin();
    for (const auto& [val, count] : dense_res) {
        std::fill_n(out_it, count, val);
        out_it += count;
    }
}

}
