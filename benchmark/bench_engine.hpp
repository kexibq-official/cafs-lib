#pragma once

#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

namespace bench {

template <typename Fn>
inline double measure_ms(Fn&& fn) {
    using clock = std::chrono::high_resolution_clock;
    auto start = clock::now();
    fn();
    auto end = clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

inline std::string csv_escape(std::string_view s) {
    bool needs = false;
    for (char c : s) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') { needs = true; break; }
    }
    if (!needs) return std::string(s);
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

struct CsvWriter {
    FILE* f = nullptr;
    bool header_written = false;

    explicit CsvWriter(const std::string& path) {

        f = std::fopen(path.c_str(), "w");
        if (!f) throw std::runtime_error("Failed to open output CSV: " + path);
    }

    ~CsvWriter() {
        if (f) std::fclose(f);
    }

    void write_header() {
        if (header_written) return;
        header_written = true;
        std::fprintf(f, "dataset,pattern,n,range,entropy_req,entropy_eff,signed,seed,algo,time_ms,ok,repeats\n");
    }

    void write_row(std::string_view dataset,
                   std::string_view pattern,
                   std::uint64_t n,
                   std::int64_t range,
                   std::uint32_t entropy_req,
                   std::uint32_t entropy_eff,
                   std::string_view signed_mode,
                   std::uint64_t seed,
                   std::string_view algo,
                   double time_ms,
                   bool ok,
                   int repeats) {

        if (!header_written) write_header();

        std::string s_dataset = csv_escape(dataset);
        std::string s_pattern = csv_escape(pattern);
        std::string s_signed = csv_escape(signed_mode);
        std::string s_algo = csv_escape(algo);

        std::fprintf(f, "%s,%s,%llu,%lld,%u,%u,%s,%llu,%s,%.4f,%s,%d\n",
            s_dataset.c_str(),
            s_pattern.c_str(),
            (unsigned long long)n,
            (long long)range,
            entropy_req,
            entropy_eff,
            s_signed.c_str(),
            (unsigned long long)seed,
            s_algo.c_str(),
            time_ms,
            ok ? "yes" : "FAIL",
            repeats
        );
        std::fflush(f);
    }
};

}
