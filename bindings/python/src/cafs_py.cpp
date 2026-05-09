#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include "cafs2.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace py = pybind11;

template <typename T>
static void sort_inplace(py::array_t<T, py::array::c_style | py::array::forcecast> arr) {
    auto buf = arr.request();
    if (buf.ndim != 1) {
        throw std::runtime_error("expected a 1-D contiguous array");
    }
    const std::size_t n = static_cast<std::size_t>(buf.size);
    if (n < 2) return;
    T* ptr = static_cast<T*>(buf.ptr);

    std::vector<T> v;
    v.reserve(n);
    v.assign(ptr, ptr + n);
    cafs2::cafs_sort(v);
    std::memcpy(ptr, v.data(), n * sizeof(T));
}

PYBIND11_MODULE(_cafs, m) {
    m.doc() = "CAFS sort: cache-aware frequency sort for low-cardinality integer arrays";
    m.def("sort_uint64", &sort_inplace<std::uint64_t>, py::arg("array"),
          "Sort a 1-D numpy.uint64 array in place.");
    m.def("sort_int64",  &sort_inplace<std::int64_t>,  py::arg("array"),
          "Sort a 1-D numpy.int64 array in place.");
    m.def("sort_int32",  &sort_inplace<std::int32_t>,  py::arg("array"),
          "Sort a 1-D numpy.int32 array in place.");
}
