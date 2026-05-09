# cafs (Rust)

Rust binding for CAFS via the `cc` crate. The build script compiles `bindings/c/cafs_c.cpp` and the C++20 headers into a static library and links it.

## Use from another crate (path dependency)

In `Cargo.toml`:

```toml
[dependencies]
cafs = { path = "../path/to/cafs-lib/bindings/rust" }
```

In code:

```rust
use cafs::sort;

fn main() {
    let mut data: Vec<u64> = vec![7, 2, 7, 9, 2, 7, 9, 9, 2, 7];
    sort(&mut data);
}
```

The single generic `sort<T: Sortable>(&mut [T])` accepts `u64`, `i64`, and `i32`. The `Sortable` trait is sealed so external types cannot implement it.

## Requirements

- x86-64 with AVX2 + BMI1
- Rust 1.65 or later
- A C++20 compiler reachable by the `cc` crate (g++ 13+, clang 16+, or MSVC 19.30+)

## Test

```
cd bindings/rust
cargo test --release
```

## Notes on layout

The build references `../c/cafs_c.cpp` and `../../include/` relative to this crate. This works for in-tree builds inside the cafs-lib repository. To publish on crates.io the C source and the two headers must be vendored into the crate directory.
