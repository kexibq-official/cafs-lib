use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let c_src = manifest_dir.join("../c/cafs_c.cpp");
    let c_inc = manifest_dir.join("../c");
    let cafs_inc = manifest_dir.join("../../include");

    let mut build = cc::Build::new();
    build
        .cpp(true)
        .file(&c_src)
        .include(&c_inc)
        .include(&cafs_inc)
        .flag_if_supported("-std=c++20")
        .flag_if_supported("/std:c++20")
        .flag_if_supported("-O3")
        .flag_if_supported("/O2")
        .flag_if_supported("-mavx2")
        .flag_if_supported("/arch:AVX2")
        .flag_if_supported("-mbmi")
        .define("NDEBUG", None);

    build.compile("cafs_c");

    println!("cargo:rerun-if-changed={}", c_src.display());
    println!("cargo:rerun-if-changed={}/cafs_c.h", c_inc.display());
    println!("cargo:rerun-if-changed={}/cafs.hpp", cafs_inc.display());
    println!("cargo:rerun-if-changed={}/cafs2.hpp", cafs_inc.display());
}
