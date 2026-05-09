#ifndef CAFS_C_H
#define CAFS_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cafs_sort_u64(uint64_t* data, size_t n);
void cafs_sort_i64(int64_t* data, size_t n);
void cafs_sort_i32(int32_t* data, size_t n);

#ifdef __cplusplus
}
#endif

#endif
