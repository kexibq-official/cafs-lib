#include "bench_engine.hpp"
#include "cafs2.hpp"
#include "competitors.hpp"
#include "datagen.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace cli {
static std::vector<std::uint64_t> parse_u64_list(std::string_view s) {
    std::vector<std::uint64_t> out;
    std::uint64_t cur = 0;
    bool have = false;
    for (char c : s) {
        if (c == ',') { if (have) out.push_back(cur); cur = 0; have = false; continue; }
        if (c >= '0' && c <= '9') { have = true; cur = cur * 10 + static_cast<std::uint64_t>(c - '0'); }
    }
    if (have) out.push_back(cur);
    return out;
}
}

static std::vector<std::uint64_t> generate_lite_sizes() {
    return { 1000, 100000, 1000000 };
}

static std::vector<std::uint64_t> generate_adaptive_sizes() {
    std::vector<std::uint64_t> s;

    for (std::uint64_t i = 1000;     i < 50000;     i += 2000)    s.push_back(i);

    for (std::uint64_t i = 50000;    i < 1000000;   i += 50000)   s.push_back(i);

    for (std::uint64_t i = 1000000;  i < 10000000;  i += 1000000) s.push_back(i);

    for (std::uint64_t i = 10000000; i <= 100000000;i += 5000000) s.push_back(i);
    std::sort(s.begin(), s.end());
    s.erase(std::unique(s.begin(), s.end()), s.end());
    return s;
}

struct Config {
    std::string                 out_csv = "results.csv";
    std::vector<std::uint64_t>  sizes;
    std::uint32_t               k_min = 2;
    std::uint32_t               k_max = 100'000'000;
    std::uint64_t               max_n = 0;
    bool                        lite_mode = false;
    bool                        only_cafs = false;
    std::vector<std::string>    algo_filter;
};

static std::vector<std::string> split_csv(std::string_view s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') { if (!cur.empty()) { out.push_back(cur); cur.clear(); } }
        else if (c != ' ') cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

static std::uint64_t count_cases(const std::vector<std::uint64_t>& sizes,
                                 std::uint32_t k_min, std::uint32_t k_max,
                                 bool lite) {
    std::uint64_t total = 0;
    for (auto n : sizes) {
        std::uint64_t k = k_min;
        while (k <= k_max) {
            if (k > n) break;
            ++total;
            std::uint64_t step;
            if (lite)                 step = k * 5;
            else if (k < 200)         step = 1;
            else if (k < 15000)       step = 10;
            else if (k < 100000)      step = 500;
            else                      step = std::max<std::uint64_t>(5000, k / 10);
            if (k + step < k) break;
            k += step;
        }
    }
    return total;
}

static std::string fmt_duration(double s) {
    if (s < 0) s = 0;
    int total = static_cast<int>(s);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int sec = total % 60;
    char buf[32];
    if (h > 0)      std::snprintf(buf, sizeof(buf), "%dh%02dm", h, m);
    else if (m > 0) std::snprintf(buf, sizeof(buf), "%dm%02ds", m, sec);
    else            std::snprintf(buf, sizeof(buf), "%ds", sec);
    return std::string(buf);
}

#if defined(__GNUC__)
__attribute__((force_align_arg_pointer))
#endif
int main(int argc, char** argv) {
    std::cout << std::unitbuf;

    try {
        using u64 = std::uint64_t;
        Config cfg;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if      (arg == "--out"       && i+1 < argc) cfg.out_csv = argv[++i];
            else if (arg == "--lite")                     cfg.lite_mode = true;
            else if (arg == "--only_cafs")                cfg.only_cafs = true;
            else if (arg == "--sizes"     && i+1 < argc) cfg.sizes = cli::parse_u64_list(argv[++i]);
            else if (arg == "--algos"     && i+1 < argc) cfg.algo_filter = split_csv(argv[++i]);
            else if (arg == "--max-n"     && i+1 < argc) cfg.max_n = std::stoull(argv[++i]);
        }

        if (cfg.sizes.empty()) {
            cfg.sizes = cfg.lite_mode ? generate_lite_sizes() : generate_adaptive_sizes();
        }
        if (cfg.max_n > 0) {
            cfg.sizes.erase(
                std::remove_if(cfg.sizes.begin(), cfg.sizes.end(),
                               [&](std::uint64_t n){ return n > cfg.max_n; }),
                cfg.sizes.end());
        }
        if (cfg.sizes.empty()) {
            std::cerr << "[ERROR] After --max-n filter no sizes remain.\n";
            return 1;
        }
        std::cout << "[INFO] N-points: " << cfg.sizes.size()
                  << " (max: " << cfg.sizes.back() << ")\n";

        auto pdq_fb = [](std::vector<u64>& v){ pdqsort(v.begin(), v.end()); };

        std::vector<std::pair<std::string, std::function<void(std::vector<u64>&)>>> algorithms;

        algorithms.emplace_back("cafs_sort", [pdq_fb](std::vector<u64>& v) {
            cafs2::cafs_sort(v, pdq_fb);
        });

        if (!cfg.only_cafs) {
            algorithms.emplace_back("std::sort", competitors::std_sort);
            if constexpr (competitors::pdqsort_available)
                algorithms.emplace_back("pdqsort", competitors::pdq_sort);
            if constexpr (competitors::ska_sort_available)
                algorithms.emplace_back("ska_sort", competitors::ska_sort_wrapper);
            if constexpr (competitors::ips4o_available)
                algorithms.emplace_back("ips4o", competitors::ips4o_sort);
            if constexpr (competitors::vqsort_available)
                algorithms.emplace_back("vqsort", competitors::vqsort);
        }

        if (!cfg.algo_filter.empty()) {
            std::vector<std::pair<std::string, std::function<void(std::vector<u64>&)>>> kept;
            for (auto& [name, fn] : algorithms) {
                for (const auto& want : cfg.algo_filter) {
                    if (name == want) { kept.emplace_back(name, fn); break; }
                }
            }
            algorithms = std::move(kept);
            if (algorithms.empty()) {
                std::cerr << "[ERROR] --algos filter matched nothing. Available: "
                          << "cafs_sort, std::sort, pdqsort, ska_sort\n";
                return 1;
            }
        }

        bench::CsvWriter csv(cfg.out_csv);

        const std::uint64_t total_cases =
            count_cases(cfg.sizes, cfg.k_min, cfg.k_max, cfg.lite_mode);
        std::cout << "[INFO] " << algorithms.size() << " algorithms, "
                  << total_cases << " (N,K) cases total.\n";

        std::uint64_t done = 0;
        const auto t_start = std::chrono::steady_clock::now();

        for (auto n : cfg.sizes) {
            std::uint64_t k = cfg.k_min;

            while (k <= cfg.k_max) {
                if (k > n) break;

                std::uint64_t seed = 42 + n + k;
                auto gen = datagen::low_entropy(
                    n, 0, static_cast<std::uint32_t>(k),
                    datagen::SignedMode::NonNegative,
                    datagen::Pattern::Random,
                    seed, 0
                );

                std::vector<u64> ref = gen.data;
                std::sort(ref.begin(), ref.end());

                for (const auto& [name, fn] : algorithms) {
                    int runs = (cfg.lite_mode ? 1 : 2);
                    double best_ms = std::numeric_limits<double>::infinity();
                    bool   all_ok  = true;

                    for (int r = 0; r < runs; ++r) {
                        std::vector<u64> data = gen.data;
                        double ms = bench::measure_ms([&](){ fn(data); });
                        if (ms < best_ms) best_ms = ms;
                        if (data != ref)  all_ok  = false;
                    }

                    csv.write_row("random", "random", n, 0,
                                  static_cast<std::uint32_t>(k),
                                  gen.entropy_eff, "nonneg", seed,
                                  name, best_ms, all_ok, runs);
                }

                ++done;
                {
                    auto t_now = std::chrono::steady_clock::now();
                    double elapsed_s = std::chrono::duration<double>(t_now - t_start).count();
                    double eta_s = (done > 0)
                        ? elapsed_s * static_cast<double>(total_cases - done) / static_cast<double>(done)
                        : 0.0;
                    double pct = total_cases ? 100.0 * done / total_cases : 0.0;
                    std::printf("\r[%5.1f%%] %llu/%llu  N=%-9llu K=%-9llu  elapsed=%s  ETA=%s        ",
                                pct,
                                (unsigned long long)done,
                                (unsigned long long)total_cases,
                                (unsigned long long)n,
                                (unsigned long long)k,
                                fmt_duration(elapsed_s).c_str(),
                                fmt_duration(eta_s).c_str());
                    std::fflush(stdout);
                }

                std::uint64_t step;
                if (cfg.lite_mode) {
                    step = k * 5;
                } else if (k < 200) {
                    step = 1;
                } else if (k < 15000) {
                    step = 10;
                } else if (k < 100000) {
                    step = 500;
                } else {
                    step = std::max<std::uint64_t>(5000, k / 10);
                }

                if (k + step < k) break;
                k += step;
            }
            std::cout << "\n  -> done N=" << n << "\n";
        }

        auto t_end = std::chrono::steady_clock::now();
        double total_s = std::chrono::duration<double>(t_end - t_start).count();
        std::cout << "\n[SUCCESS] " << done << " cases in "
                  << fmt_duration(total_s) << ". CSV: " << cfg.out_csv << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
