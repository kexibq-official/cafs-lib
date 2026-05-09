#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#if __has_include("third_party/pdqsort.h")
  #include "third_party/pdqsort.h"
  #define CAFS_HAVE_PDQSORT 1
#elif __has_include("pdqsort.h")
  #include "pdqsort.h"
  #define CAFS_HAVE_PDQSORT 1
#else
  #define CAFS_HAVE_PDQSORT 0
#endif

#if __has_include("third_party/ska_sort.hpp")
  #include "third_party/ska_sort.hpp"
  #define CAFS_HAVE_SKA_SORT 1
#elif __has_include("ska_sort.hpp")
  #include "ska_sort.hpp"
  #define CAFS_HAVE_SKA_SORT 1
#else
  #define CAFS_HAVE_SKA_SORT 0
#endif

#if __has_include(<boost/sort/spreadsort/integer_sort.hpp>)
  #include <boost/sort/spreadsort/integer_sort.hpp>
  #define CAFS_HAVE_BOOST_SPREADSORT 1
#else
  #define CAFS_HAVE_BOOST_SPREADSORT 0
#endif

#if __has_include("third_party/unordered_dense.h")
  #include "third_party/unordered_dense.h"
  #define CAFS_HAVE_UNORDERED_DENSE 1
#elif __has_include("unordered_dense.h")
  #include "unordered_dense.h"
  #define CAFS_HAVE_UNORDERED_DENSE 1
#else
  #define CAFS_HAVE_UNORDERED_DENSE 0
#endif

#if __has_include("third_party/ips4o_repo/include/ips4o.hpp")
  #include "third_party/ips4o_repo/include/ips4o.hpp"
  #define CAFS_HAVE_IPS4O 1
#else
  #define CAFS_HAVE_IPS4O 0
#endif

#if __has_include("third_party/highway_repo/hwy/contrib/sort/vqsort.h")
  #include "third_party/highway_repo/hwy/contrib/sort/vqsort.h"
  #define CAFS_HAVE_VQSORT 1
#else
  #define CAFS_HAVE_VQSORT 0
#endif

namespace competitors {

using u64 = std::uint64_t;

constexpr bool pdqsort_available = CAFS_HAVE_PDQSORT == 1;
constexpr bool ska_sort_available = CAFS_HAVE_SKA_SORT == 1;
constexpr bool boost_spreadsort_available = CAFS_HAVE_BOOST_SPREADSORT == 1;
constexpr bool unordered_dense_available = CAFS_HAVE_UNORDERED_DENSE == 1;
constexpr bool ips4o_available = CAFS_HAVE_IPS4O == 1;
constexpr bool vqsort_available = CAFS_HAVE_VQSORT == 1;

inline void std_sort(std::vector<u64>& v) {
    std::sort(v.begin(), v.end());
}

inline void pdq_sort(std::vector<u64>& v) {
#if CAFS_HAVE_PDQSORT
    pdqsort(v.begin(), v.end());
#else
    throw std::runtime_error("pdqsort missing");
#endif
}

inline void ska_sort_wrapper(std::vector<u64>& v) {
#if CAFS_HAVE_SKA_SORT
    ska_sort(v.begin(), v.end());
#else
    throw std::runtime_error("ska_sort missing");
#endif
}

inline void boost_spreadsort(std::vector<u64>& v) {
#if CAFS_HAVE_BOOST_SPREADSORT

    boost::sort::spreadsort::integer_sort(v.begin(), v.end());
#else
    throw std::runtime_error("boost headers missing");
#endif
}

inline void dense_map_sort(std::vector<u64>& v) {
#if CAFS_HAVE_UNORDERED_DENSE
    if (v.empty()) return;

    ankerl::unordered_dense::map<u64, std::uint32_t> counts;

    counts.reserve(v.size() / 8 + 8);

    for (u64 x : v) ++counts[x];

    std::vector<std::pair<u64, std::uint32_t>> flat;
    flat.reserve(counts.size());
    for (const auto& kv : counts) flat.push_back({kv.first, kv.second});

    std::sort(flat.begin(), flat.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    auto it = v.begin();
    for (const auto& p : flat) {
        std::size_t c = p.second;
        std::fill_n(it, c, p.first);
        it += c;
    }
#else
    throw std::runtime_error("unordered_dense.h missing");
#endif
}

inline void ips4o_sort(std::vector<u64>& v) {
#if CAFS_HAVE_IPS4O
    ips4o::sort(v.begin(), v.end());
#else
    throw std::runtime_error("ips4o headers missing");
#endif
}

inline void vqsort(std::vector<u64>& v) {
#if CAFS_HAVE_VQSORT
    if (!v.empty()) {
        hwy::VQSort(v.data(), v.size(), hwy::SortAscending());
    }
#else
    throw std::runtime_error("highway vqsort not built");
#endif
}

}
