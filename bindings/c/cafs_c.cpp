#include "cafs_c.h"
#include "cafs2.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

template <typename T>
inline void sort_pointer(T* data, size_t n) {
    if (n < 2) return;
    std::vector<T> v;
    v.reserve(n);
    v.assign(data, data + n);
    cafs2::cafs_sort(v);
    std::memcpy(data, v.data(), n * sizeof(T));
}

}

extern "C" {

void cafs_sort_u64(uint64_t* data, size_t n) { sort_pointer<uint64_t>(data, n); }
void cafs_sort_i64(int64_t*  data, size_t n) { sort_pointer<int64_t>(data, n); }
void cafs_sort_i32(int32_t*  data, size_t n) { sort_pointer<int32_t>(data, n); }

}
