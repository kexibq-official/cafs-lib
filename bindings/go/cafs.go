package cafs

/*
#cgo CXXFLAGS: -O3 -march=haswell -std=c++20 -DNDEBUG -I${SRCDIR}
#cgo LDFLAGS: -lstdc++
#include "cafs_shim.h"
*/
import "C"

import "unsafe"

type Sortable interface {
	uint64 | int64 | int32
}

func Sort[T Sortable](data []T) {
	n := len(data)
	if n < 2 {
		return
	}
	switch d := any(data).(type) {
	case []uint64:
		C.cafs_sort_u64((*C.uint64_t)(unsafe.Pointer(&d[0])), C.size_t(n))
	case []int64:
		C.cafs_sort_i64((*C.int64_t)(unsafe.Pointer(&d[0])), C.size_t(n))
	case []int32:
		C.cafs_sort_i32((*C.int32_t)(unsafe.Pointer(&d[0])), C.size_t(n))
	}
}
