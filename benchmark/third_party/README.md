# Third-party dependencies

The benchmark links against four external sorts. competitors.hpp uses __has_include to detect each one and substitutes a runtime stub when missing.

pdqsort.h is also referenced directly in main_bigdata.cpp as the CAFS dispatcher fallback, so it is required for the benchmark to compile. The other three are optional: the benchmark builds with whichever subset is available.

## pdqsort (required)

Single header. Place at third_party/pdqsort.h.

```
curl -L https://raw.githubusercontent.com/orlp/pdqsort/master/pdqsort.h \
    -o third_party/pdqsort.h
```

## ska_sort (optional)

Single header. Place at third_party/ska_sort.hpp.

```
curl -L https://raw.githubusercontent.com/skarupke/ska_sort/master/ska_sort.hpp \
    -o third_party/ska_sort.hpp
```

## IPS4o (optional)

Header-only repository with internal includes. Place at third_party/ips4o_repo.

```
git clone https://github.com/ips4o/ips4o.git third_party/ips4o_repo
```

IPS4o pulls in TBB types in its include graph even on sequential builds. Either install TBB headers system-wide, or place a thin TBB stub on the include path that declares the namespaces tbb::task_group, tbb::concurrent_vector, etc., as empty types. The benchmark calls only ips4o::sort and never the parallel entry points.

## Google Highway / vqsort (optional)

CMake-based repository. Place at third_party/highway_repo.

```
git clone https://github.com/google/highway.git third_party/highway_repo
```

The top-level benchmark/CMakeLists.txt picks up the Highway tree via add_subdirectory and exposes the hwy and hwy_contrib targets. To pin the AVX2 dispatch path and skip AVX-512 on supporting CPUs, define HWY_COMPILE_ONLY_STATIC at compile time.
