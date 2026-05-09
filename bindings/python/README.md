# cafs (Python)

Python binding for CAFS using pybind11. Operates on 1-D numpy integer arrays in place.

## Install

From the repository root:

```
pip install ./bindings/python
```

Requirements: x86-64 with AVX2 + BMI1, C++20 toolchain (g++ 13 or later, clang 16 or later, MSVC 19.30 or later), Python 3.8 or later, numpy.

## Usage

```python
import numpy as np
import cafs

data = np.random.default_rng(0).integers(0, 1000, size=1_000_000, dtype=np.uint64)
cafs.sort(data)
```

The single `cafs.sort(array)` dispatches at runtime by `array.dtype`. Supported dtypes: `uint64`, `int64`, `int32`. Other integer dtypes are upcast to `uint64` and copied back. The function modifies the array in place and also returns it.

## Test

```
pip install pytest
pytest bindings/python/tests
```
