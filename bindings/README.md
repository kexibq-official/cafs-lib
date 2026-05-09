# Language bindings

Thin wrappers around the C++20 core for Python, Rust, and Go. All three expose a single `sort` entry point and call into a small C-API (`bindings/c/cafs_c.{h,cpp}`) that owns the temporary `std::vector` used by the dispatcher; data passes through a single allocation plus a `memcpy` round trip.

| Binding | Module name | Single API                         | Path                |
|---------|-------------|------------------------------------|---------------------|
| C       | n/a         | `cafs_sort_u64/i64/i32(ptr, n)`    | `bindings/c`        |
| Python  | `cafs`      | `cafs.sort(np.ndarray)`            | `bindings/python`   |
| Rust    | `cafs`      | `cafs::sort(&mut [T])` (generic)   | `bindings/rust`     |
| Go      | `cafs`      | `cafs.Sort(data)` (generic)        | `bindings/go`       |

Supported element types in all three bindings: `uint64`, `int64`, `int32`. All bindings target x86-64 with AVX2 + BMI1.

## Why a copy round trip in the wrappers

The C++ entry point is `cafs2::cafs_sort(std::vector<T>&)`. To keep the core API stable, the C wrapper builds a `std::vector` from the caller pointer, runs the sort, then copies back. The overhead is a single allocation plus two passes over the array (read on entry, write on exit), which is dominated by the sort itself for any `N > ~10^4`. If a zero-copy path becomes necessary, expose a pointer + size overload in `cafs2.hpp` and route the bindings to it.
