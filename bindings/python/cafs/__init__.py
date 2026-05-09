from __future__ import annotations

import numpy as np

from . import _cafs

__all__ = ["sort"]
__version__ = "0.1.0"


_DTYPE_DISPATCH = {
    np.dtype("uint64"): _cafs.sort_uint64,
    np.dtype("int64"):  _cafs.sort_int64,
    np.dtype("int32"):  _cafs.sort_int32,
}


def sort(array: np.ndarray) -> np.ndarray:
    """Sort a 1-D numpy integer array in place and return it.

    Supported dtypes: uint64, int64, int32. Other integer dtypes are upcast to
    uint64. The array must be 1-D and contiguous; non-contiguous input is
    copied internally by the binding.
    """
    if not isinstance(array, np.ndarray):
        raise TypeError(f"expected numpy.ndarray, got {type(array).__name__}")
    if array.ndim != 1:
        raise ValueError(f"expected 1-D array, got {array.ndim}-D")

    fn = _DTYPE_DISPATCH.get(array.dtype)
    if fn is None:
        if not np.issubdtype(array.dtype, np.integer):
            raise TypeError(f"unsupported dtype: {array.dtype} (need integer)")
        cast = array.astype(np.uint64, copy=False)
        _cafs.sort_uint64(cast)
        if cast is not array:
            np.copyto(array, cast.astype(array.dtype, copy=False))
        return array

    fn(array)
    return array
