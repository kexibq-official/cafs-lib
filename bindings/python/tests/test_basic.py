import numpy as np

import cafs


def _palette(K, dtype, seed):
    rng = np.random.default_rng(seed)
    if dtype == np.uint64:
        return rng.integers(0, np.iinfo(np.uint64).max, size=K, dtype=np.uint64)
    if dtype == np.int64:
        return rng.integers(np.iinfo(np.int64).min,
                            np.iinfo(np.int64).max, size=K, dtype=np.int64)
    if dtype == np.int32:
        return rng.integers(np.iinfo(np.int32).min,
                            np.iinfo(np.int32).max, size=K, dtype=np.int32)
    raise AssertionError(dtype)


def _build(N, K, dtype, seed):
    rng = np.random.default_rng(seed + 1)
    palette = _palette(K, dtype, seed)
    idx = rng.integers(0, K, size=N)
    return palette[idx].astype(dtype, copy=False)


def _check(N, K, dtype):
    data = _build(N, K, dtype, seed=42 + N + K)
    ref = np.sort(data)
    cafs.sort(data)
    assert np.array_equal(data, ref), f"mismatch at dtype={dtype} N={N} K={K}"


def test_uint64_main_path():
    _check(1_000_000, 1_000, np.uint64)


def test_uint64_tiny():
    _check(1_000_000, 4, np.uint64)


def test_uint64_high_k():
    _check(1_000_000, 100_000, np.uint64)


def test_int64_main_path():
    _check(500_000, 500, np.int64)


def test_int32_main_path():
    _check(500_000, 500, np.int32)


def test_empty():
    arr = np.array([], dtype=np.uint64)
    cafs.sort(arr)
    assert arr.size == 0


def test_single():
    arr = np.array([42], dtype=np.uint64)
    cafs.sort(arr)
    assert arr[0] == 42
