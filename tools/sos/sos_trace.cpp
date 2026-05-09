#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace trace {

static FILE* g_log = nullptr;
static std::size_t g_seq = 0;

inline void open_log(const char* path) {
    g_log = std::fopen(path, "w");
    std::fprintf(g_log, "[\n");
}

inline void close_log() {
    if (!g_log) return;
    if (g_seq > 0) std::fprintf(g_log, ",\n");
    std::fprintf(g_log, "  {\"seq\": %zu, \"op\": \"END\"}\n]\n", g_seq++);
    std::fclose(g_log);
    g_log = nullptr;
}

inline void event(const char* fmt, ...) {
    if (!g_log) return;
    if (g_seq > 0) std::fprintf(g_log, ",\n");
    std::fprintf(g_log, "  {\"seq\": %zu, ", g_seq++);
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_log, fmt, ap);
    va_end(ap);
    std::fprintf(g_log, "}");
}

}

namespace cafs_vis {

constexpr int CAP = 4;

struct Bucket {
    std::uint64_t keys[CAP] = {0, 0, 0, 0};
    std::uint32_t counts[CAP] = {0, 0, 0, 0};

    int find_or_insert(std::uint64_t v, std::uint32_t inc) {
        for (int i = 0; i < CAP; ++i) {
            if (counts[i] > 0 && keys[i] == v) {
                counts[i] += inc;
                return i;
            }
        }
        for (int i = 0; i < CAP; ++i) {
            if (counts[i] == 0) {
                keys[i] = v;
                counts[i] = inc;
                return i;
            }
        }
        return -1;
    }
};

inline std::size_t fast_hash(std::uint64_t v, int shift) {
    return (v * 11400714819323198485ULL) >> shift;
}

struct SampleResult {
    std::size_t K_est;
    std::size_t sample_unique;
};

SampleResult estimate_k(const std::vector<std::uint64_t>& data,
                        std::size_t sample_n) {
    const std::size_t N = data.size();
    const std::size_t stride = std::max<std::size_t>(1, N / sample_n);
    std::vector<std::uint64_t> seen;
    std::vector<int> count;
    std::size_t taken = 0;
    for (std::size_t i = 0; i < N && taken < sample_n; i += stride, ++taken) {
        std::uint64_t v = data[i];
        trace::event("\"op\": \"SAMPLE\", \"i\": %zu, \"v\": %llu",
                     i, (unsigned long long)v);
        bool found = false;
        for (std::size_t j = 0; j < seen.size(); ++j) {
            if (seen[j] == v) { count[j]++; found = true; break; }
        }
        if (!found) { seen.push_back(v); count.push_back(1); }
    }
    std::size_t f1 = 0, f2 = 0;
    for (int c : count) { if (c == 1) ++f1; else if (c == 2) ++f2; }
    std::size_t bias = (f1 * f1) / (2 * (f2 + 1));
    std::size_t K_est = seen.size() + bias;
    if (K_est > N) K_est = N;
    trace::event("\"op\": \"CHAO1\", \"u\": %zu, \"f1\": %zu, \"f2\": %zu, \"K_est\": %zu",
                 seen.size(), f1, f2, K_est);
    return {K_est, seen.size()};
}

void cafs_sort_traced(std::vector<std::uint64_t>& data, std::size_t M) {
    const std::size_t N = data.size();
    const std::size_t mask = M - 1;
    const int shift = 64 - __builtin_ctzll(M);

    std::vector<Bucket> table(M);
    std::vector<std::uint64_t> spill;

    trace::event("\"op\": \"PHASE\", \"name\": \"hot_loop\", \"M\": %zu", M);

    std::size_t i = 0;
    while (i < N) {
        std::uint64_t val = data[i];
        std::size_t run_len = 1;
        while (i + run_len < N && data[i + run_len] == val) ++run_len;

        std::size_t idx = fast_hash(val, shift) & mask;
        int slot_before = -1;
        bool was_hit = false;
        for (int s = 0; s < CAP; ++s) {
            if (table[idx].counts[s] > 0 && table[idx].keys[s] == val) {
                slot_before = s; was_hit = true; break;
            }
        }
        int slot = table[idx].find_or_insert(val, run_len);
        if (slot < 0) {
            trace::event("\"op\": \"SPILL\", \"i\": %zu, \"v\": %llu, \"run\": %zu",
                         i, (unsigned long long)val, run_len);
            for (std::size_t k = 0; k < run_len; ++k) spill.push_back(val);
        } else {
            trace::event("\"op\": \"BUCKET\", \"i\": %zu, \"v\": %llu, \"bucket\": %zu, "
                         "\"slot\": %d, \"hit\": %d, \"run\": %zu, \"new_count\": %u",
                         i, (unsigned long long)val, idx, slot, was_hit ? 1 : 0,
                         run_len, table[idx].counts[slot]);
        }
        i += run_len;
    }

    trace::event("\"op\": \"PHASE\", \"name\": \"reconstruct\"");

    std::vector<std::pair<std::uint64_t, std::uint32_t>> dense;
    for (std::size_t b = 0; b < M; ++b) {
        for (int s = 0; s < CAP; ++s) {
            if (table[b].counts[s] > 0) {
                dense.emplace_back(table[b].keys[s], table[b].counts[s]);
                trace::event("\"op\": \"COLLECT\", \"bucket\": %zu, \"slot\": %d, "
                             "\"key\": %llu, \"count\": %u",
                             b, s, (unsigned long long)table[b].keys[s],
                             table[b].counts[s]);
            }
        }
    }
    if (!spill.empty()) {
        std::sort(spill.begin(), spill.end());
        std::uint64_t cur = spill[0];
        std::uint32_t c = 1;
        for (std::size_t k = 1; k < spill.size(); ++k) {
            if (spill[k] == cur) ++c;
            else { dense.emplace_back(cur, c); cur = spill[k]; c = 1; }
        }
        dense.emplace_back(cur, c);
    }

    std::sort(dense.begin(), dense.end(),
              [](auto& a, auto& b){ return a.first < b.first; });
    for (std::size_t j = 0; j < dense.size(); ++j) {
        trace::event("\"op\": \"PAIR\", \"rank\": %zu, \"key\": %llu, \"count\": %u",
                     j, (unsigned long long)dense[j].first, dense[j].second);
    }

    trace::event("\"op\": \"PHASE\", \"name\": \"emit\"");

    auto out = data.begin();
    for (auto& p : dense) {
        for (std::uint32_t k = 0; k < p.second; ++k) {
            std::size_t pos = (out - data.begin()) + k;
            trace::event("\"op\": \"EMIT\", \"pos\": %zu, \"key\": %llu",
                         pos, (unsigned long long)p.first);
        }
        std::fill_n(out, p.second, p.first);
        out += p.second;
    }
}

}

int main(int argc, char** argv) {
    std::size_t N = 256;
    std::size_t K = 8;
    std::uint64_t seed = 7;
    std::size_t M = 32;
    const char* out = "events.json";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--N"    && i+1 < argc) N    = std::stoull(argv[++i]);
        else if (a == "--K"    && i+1 < argc) K    = std::stoull(argv[++i]);
        else if (a == "--seed" && i+1 < argc) seed = std::stoull(argv[++i]);
        else if (a == "--M"    && i+1 < argc) M    = std::stoull(argv[++i]);
        else if (a == "--out"  && i+1 < argc) out  = argv[++i];
    }

    std::mt19937_64 rng(seed);
    std::vector<std::uint64_t> palette(K);
    for (std::size_t i = 0; i < K; ++i) palette[i] = (rng() & 0xFFFF) + 1;
    std::sort(palette.begin(), palette.end());
    palette.erase(std::unique(palette.begin(), palette.end()), palette.end());
    K = palette.size();

    std::vector<std::uint64_t> data(N);
    std::uniform_int_distribution<std::size_t> pick(0, K - 1);
    for (auto& x : data) x = palette[pick(rng)];

    trace::open_log(out);
    trace::event("\"op\": \"INIT\", \"N\": %zu, \"K\": %zu, \"seed\": %llu, \"M\": %zu",
                 N, K, (unsigned long long)seed, M);
    {
        std::string arr = "[";
        for (std::size_t i = 0; i < N; ++i) {
            if (i) arr += ",";
            arr += std::to_string(data[i]);
        }
        arr += "]";
        trace::event("\"op\": \"INPUT\", \"data\": %s", arr.c_str());
    }
    {
        std::string pal = "[";
        for (std::size_t i = 0; i < K; ++i) {
            if (i) pal += ",";
            pal += std::to_string(palette[i]);
        }
        pal += "]";
        trace::event("\"op\": \"PALETTE\", \"keys\": %s", pal.c_str());
    }

    cafs_vis::estimate_k(data, 64);
    cafs_vis::cafs_sort_traced(data, M);

    {
        std::string arr = "[";
        for (std::size_t i = 0; i < N; ++i) {
            if (i) arr += ",";
            arr += std::to_string(data[i]);
        }
        arr += "]";
        trace::event("\"op\": \"OUTPUT\", \"data\": %s", arr.c_str());
    }

    trace::close_log();
    std::printf("wrote %s (N=%zu, K=%zu, M=%zu)\n", out, N, K, M);
    return 0;
}
