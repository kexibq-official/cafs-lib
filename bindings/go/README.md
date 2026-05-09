# cafs (Go)

Go binding for CAFS via cgo. The package vendors the C++20 sources and headers so that `go get` works without external paths.

## Install

```
go get github.com/kexibq-official/cafs-lib/bindings/go
```

## Usage

```go
package main

import (
    "fmt"
    cafs "github.com/kexibq-official/cafs-lib/bindings/go"
)

func main() {
    data := []uint64{7, 2, 7, 9, 2, 7, 9, 9, 2, 7}
    cafs.Sort(data)
    fmt.Println(data)
}
```

The single generic `cafs.Sort[T Sortable](data []T)` accepts `[]uint64`, `[]int64`, and `[]int32`. The constraint `Sortable` rejects other types at compile time.

## Requirements

- x86-64 with AVX2 + BMI1
- Go 1.21 or later
- A C++20 compiler reachable by cgo (g++ 13+ on Linux/MinGW, clang 16+, or MSVC; MSVC requires `CC`/`CXX` set up)
- `CGO_ENABLED=1` (default on most platforms)

## Test

```
cd bindings/go
go test -v
```
