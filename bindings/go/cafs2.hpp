#pragma once

#include "cafs.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#ifndef CAFS2_TRACE
#define CAFS2_TRACE 0
#endif

namespace cafs2 {

using cafs::CafsParams;
namespace d = cafs::detail;

namespace internal {

template <typename T>
struct FreqSet {
    static constexpr std::size_t SLOTS = 4096;
    static constexpr std::size_t MASK  = SLOTS - 1;
    T              keys[SLOTS];
    std::uint16_t  cnt[SLOTS];

    FreqSet() { std::memset(cnt, 0, sizeof(cnt)); }

    inline void inc(T v) {
        std::size_t h = static_cast<std::size_t>(
            d::fast_hash(d::as_u64(v), 64 - 12)) & MASK;
        for (std::size_t probe = 0; probe < SLOTS; ++probe) {
            std::size_t idx = (h + probe) & MASK;
            if (cnt[idx] == 0) { keys[idx] = v; cnt[idx] = 1; return; }
            if (keys[idx] == v) { if (cnt[idx] < 65535) ++cnt[idx]; return; }
        }
    }
};

struct SampleResult {
    std::size_t K_est;
    std::size_t sample_n;
    std::size_t sample_unique;
    bool        all_unique;
};

template <typename T>
inline SampleResult estimate_k(const T* data, std::size_t N) {
    constexpr std::size_t SAMPLE = 1024;
    const std::size_t s = std::min<std::size_t>(SAMPLE, N);

    auto* fs = new FreqSet<T>();
    const std::size_t stride = std::max<std::size_t>(1, N / s);
    std::size_t taken = 0;
    for (std::size_t i = 0; i < N && taken < s; i += stride, ++taken)
        fs->inc(data[i]);

    std::size_t uniq = 0, f1 = 0, f2 = 0;
    for (std::size_t i = 0; i < FreqSet<T>::SLOTS; ++i) {
        std::uint16_t c = fs->cnt[i];
        if (c == 0) continue;
        ++uniq;
        if (c == 1) ++f1;
        else if (c == 2) ++f2;
    }
    delete fs;

    SampleResult r;
    r.sample_n = taken;
    r.sample_unique = uniq;
    r.all_unique = (uniq == taken);

    if (r.all_unique) {

        r.K_est = N;
    } else {

        std::uint64_t bias = (static_cast<std::uint64_t>(f1) * f1) /
                             (2ULL * (f2 + 1));
        std::uint64_t est = uniq + bias;
        if (est > N) est = N;
        if (est < uniq) est = uniq;
        r.K_est = static_cast<std::size_t>(est);
    }
    return r;
}

template <typename T>
inline bool tiny_count_sort_branchless(std::vector<T>& data,
                                        const T* keys_in, int n_keys) {
    const std::size_t N = data.size();
    T k0=0,k1=0,k2=0,k3=0,k4=0,k5=0,k6=0,k7=0;
    if (n_keys >= 1) k0 = keys_in[0];
    if (n_keys >= 2) k1 = keys_in[1];
    if (n_keys >= 3) k2 = keys_in[2];
    if (n_keys >= 4) k3 = keys_in[3];
    if (n_keys >= 5) k4 = keys_in[4];
    if (n_keys >= 6) k5 = keys_in[5];
    if (n_keys >= 7) k6 = keys_in[6];
    if (n_keys >= 8) k7 = keys_in[7];

    std::uint64_t c0=0,c1=0,c2=0,c3=0,c4=0,c5=0,c6=0,c7=0;
    const T* p = data.data();

    for (std::size_t i = 0; i < N; ++i) {
        T v = p[i];
        c0 += (v == k0);
        c1 += (v == k1);
        c2 += (v == k2);
        c3 += (v == k3);
        c4 += (v == k4);
        c5 += (v == k5);
        c6 += (v == k6);
        c7 += (v == k7);
    }

    std::uint64_t used_counts[8] = {c0,c1,c2,c3,c4,c5,c6,c7};

    std::uint64_t sum = 0;
    for (int i = 0; i < n_keys; ++i) sum += used_counts[i];

    for (int i = n_keys; i < 8; ++i) (void)used_counts[i];

    if (sum != N) return false;

    int order[8] = {0,1,2,3,4,5,6,7};
    std::sort(order, order + n_keys,
              [&](int a, int b) { return keys_in[a] < keys_in[b]; });

    auto out = data.begin();
    for (int j = 0; j < n_keys; ++j) {
        std::fill_n(out, used_counts[order[j]], keys_in[order[j]]);
        out += used_counts[order[j]];
    }
    return true;
}

template <typename T>
inline void radix_sort_pairs_by_key(std::vector<std::pair<T, std::uint32_t>>& v) {
    using Pair = std::pair<T, std::uint32_t>;
    const std::size_t n = v.size();
    if (n < 256) {
        std::sort(v.begin(), v.end(),
                  [](const Pair& a, const Pair& b){ return a.first < b.first; });
        return;
    }

    std::vector<Pair> buf(n);
    Pair* src = v.data();
    Pair* dst = buf.data();

    constexpr int BYTES = sizeof(T);
    constexpr bool is_signed_t = std::is_signed_v<T>;
    for (int byte = 0; byte < BYTES; ++byte) {
        std::size_t hist[257] = {};
        const int shift = byte * 8;
        const std::uint8_t flip =
            (is_signed_t && byte == BYTES - 1) ? std::uint8_t{0x80} : std::uint8_t{0x00};
        for (std::size_t i = 0; i < n; ++i) {
            std::uint8_t b = static_cast<std::uint8_t>(
                (static_cast<std::uint64_t>(src[i].first) >> shift) & 0xff);
            b ^= flip;
            ++hist[static_cast<std::size_t>(b) + 1];
        }
        for (int i = 1; i < 257; ++i) hist[i] += hist[i-1];

        for (std::size_t i = 0; i < n; ++i) {
            std::uint8_t b = static_cast<std::uint8_t>(
                (static_cast<std::uint64_t>(src[i].first) >> shift) & 0xff);
            b ^= flip;
            dst[hist[b]++] = src[i];
        }
        std::swap(src, dst);
    }

    if constexpr ((BYTES & 1) != 0) {
        std::copy(src, src + n, v.data());
    }
}

template <typename T>
inline std::size_t pick_num_buckets(std::size_t K_est, std::size_t N) {
    constexpr std::size_t cap = cafs::detail::Bucket<T>{}.CAPACITY;

    std::size_t target_slots = K_est * 8;
    if (target_slots < 64) target_slots = 64;
    std::size_t target_buckets = (target_slots + cap - 1) / cap;
    std::size_t nb = std::bit_ceil(target_buckets);

    std::size_t max_b = std::bit_ceil(std::max<std::size_t>(N / cap, 8));
    if (nb > max_b) nb = max_b;
    if (nb < 8) nb = 8;
    return nb;
}

}

struct DefaultFallback {
    template <typename T>
    void operator()(std::vector<T>& v) const {
        std::sort(v.begin(), v.end());
    }
};

template <typename T, typename Fallback = DefaultFallback>
void cafs_sort(std::vector<T>& data, Fallback fb = Fallback{}) {
    using namespace cafs::detail;
    const std::size_t N = data.size();
    if (N < 2) return;

    int order = check_order(data.data(), N);
    if (order ==  1) return;
    if (order == -1) { std::reverse(data.begin(), data.end()); return; }

    if (N < 2048) { fb(data); return; }

#if CAFS2_TRACE
    auto t0 = std::chrono::steady_clock::now();
#endif
    auto sr = internal::estimate_k(data.data(), N);
#if CAFS2_TRACE
    auto t1 = std::chrono::steady_clock::now();
    std::printf("[trace] N=%zu K_est=%zu uniq_in_sample=%zu all_uniq=%d sample_ms=%.2f\n",
        N, sr.K_est, sr.sample_unique, (int)sr.all_unique,
        std::chrono::duration<double, std::milli>(t1-t0).count());
#endif

    if (sr.K_est <= 8 && sr.sample_unique <= 8) {

        T keys[8];
        int n_keys = 0;
        const std::size_t s = std::min<std::size_t>(1024, N);
        const std::size_t stride = std::max<std::size_t>(1, N / s);
        for (std::size_t i = 0, t = 0; i < N && t < s && n_keys < 8;
             i += stride, ++t) {
            T v = data[i];
            bool seen = false;
            for (int j = 0; j < n_keys; ++j) if (keys[j] == v) { seen = true; break; }
            if (!seen) keys[n_keys++] = v;
        }

        if (n_keys >= 1) {
            T pad = keys[0];
            for (int i = n_keys; i < 8; ++i) keys[i] = pad;

            std::uint64_t c[8] = {};
            const T* p = data.data();
            T k0=keys[0],k1=keys[1],k2=keys[2],k3=keys[3],
              k4=keys[4],k5=keys[5],k6=keys[6],k7=keys[7];
            for (std::size_t i = 0; i < N; ++i) {
                T v = p[i];
                c[0] += (v == k0);
                c[1] += (v == k1);
                c[2] += (v == k2);
                c[3] += (v == k3);
                c[4] += (v == k4);
                c[5] += (v == k5);
                c[6] += (v == k6);
                c[7] += (v == k7);
            }

            std::uint64_t sum = 0;
            for (int i = 0; i < n_keys; ++i) sum += c[i];

            if (sum == N) {
                int idx[8] = {0,1,2,3,4,5,6,7};
                std::sort(idx, idx + n_keys,
                          [&](int a, int b) { return keys[a] < keys[b]; });
                auto out = data.begin();
                for (int j = 0; j < n_keys; ++j) {
                    std::fill_n(out, c[idx[j]], keys[idx[j]]);
                    out += c[idx[j]];
                }
                return;
            }

        }
    }

    if (sr.all_unique || sr.K_est * 2 > N) {
        fb(data);
        return;
    }

    const std::size_t num_buckets = internal::pick_num_buckets<T>(sr.K_est, N);
    const std::size_t bucket_mask = num_buckets - 1;
    const int hash_shift = 64 - std::bit_width(bucket_mask);

    auto* table = static_cast<Bucket<T>*>(
        aligned_alloc_wrapper(64, num_buckets * sizeof(Bucket<T>)));
    if (!table) { fb(data); return; }
    std::memset(table, 0, num_buckets * sizeof(Bucket<T>));

    std::vector<T> spill;
    spill.reserve(std::min<std::size_t>(N >> 4, sr.K_est * 8));
#if CAFS2_TRACE
    auto t2 = std::chrono::steady_clock::now();
    std::printf("[trace] num_buckets=%zu table_kb=%zu alloc_ms=%.2f\n",
        num_buckets, num_buckets*sizeof(Bucket<T>)/1024,
        std::chrono::duration<double, std::milli>(t2-t1).count());
#endif

    const T*    ptr = data.data();
    std::size_t i   = 0;
    while (i < N) {
        T val = ptr[i];
        std::size_t run_len = 1;
        while (i + run_len < N && ptr[i + run_len] == val) ++run_len;

        std::size_t idx = fast_hash(as_u64(val), hash_shift) & bucket_mask;
        if (!table[idx].update(val, static_cast<std::uint32_t>(run_len))) [[unlikely]] {
            for (std::size_t k = 0; k < run_len; ++k) spill.push_back(val);
        }
        i += run_len;
    }

#if CAFS2_TRACE
    auto t3 = std::chrono::steady_clock::now();
    std::printf("[trace] hot_loop_ms=%.2f spill_size=%zu\n",
        std::chrono::duration<double, std::milli>(t3-t2).count(), spill.size());
#endif
    std::vector<std::pair<T, std::uint32_t>> dense_res;
    dense_res.reserve(num_buckets * Bucket<T>::CAPACITY / 2);

    for (std::size_t b = 0; b < num_buckets; ++b) {
        const auto& bk = table[b];
        for (int s = 0; s < Bucket<T>::CAPACITY; ++s)
            if (bk.counts[s] > 0)
                dense_res.emplace_back(bk.keys[s], bk.counts[s]);
    }
    aligned_free_wrapper(table);

    if (spill.size() > N / 2) {
        fb(data);
        return;
    }

    if (!spill.empty()) {
        std::sort(spill.begin(), spill.end());
        T cur = spill[0];
        std::uint32_t c = 1;
        for (std::size_t k = 1; k < spill.size(); ++k) {
            if (spill[k] == cur) ++c;
            else { dense_res.emplace_back(cur, c); cur = spill[k]; c = 1; }
        }
        dense_res.emplace_back(cur, c);
    }

    internal::radix_sort_pairs_by_key(dense_res);

#if CAFS2_TRACE
    auto t4 = std::chrono::steady_clock::now();
    std::printf("[trace] reconstruct_setup_ms=%.2f dense_res_n=%zu\n",
        std::chrono::duration<double, std::milli>(t4-t3).count(), dense_res.size());
#endif
    auto out = data.begin();
    for (const auto& [v, c] : dense_res) {
        std::fill_n(out, c, v);
        out += c;
    }
#if CAFS2_TRACE
    auto t5 = std::chrono::steady_clock::now();
    std::printf("[trace] writeback_ms=%.2f total_ms=%.2f\n",
        std::chrono::duration<double, std::milli>(t5-t4).count(),
        std::chrono::duration<double, std::milli>(t5-t0).count());
#endif
}

}
