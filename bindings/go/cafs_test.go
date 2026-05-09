package cafs

import (
	"math/rand"
	"sort"
	"testing"
)

func buildLowCardU64(n, k int, seed int64) []uint64 {
	rng := rand.New(rand.NewSource(seed))
	palette := make([]uint64, k)
	for i := range palette {
		palette[i] = rng.Uint64()
	}
	out := make([]uint64, n)
	for i := range out {
		out[i] = palette[rng.Intn(k)]
	}
	return out
}

func buildLowCardI64(n, k int, seed int64) []int64 {
	rng := rand.New(rand.NewSource(seed))
	palette := make([]int64, k)
	for i := range palette {
		palette[i] = rng.Int63() - (1 << 62)
	}
	out := make([]int64, n)
	for i := range out {
		out[i] = palette[rng.Intn(k)]
	}
	return out
}

func buildLowCardI32(n, k int, seed int64) []int32 {
	rng := rand.New(rand.NewSource(seed))
	palette := make([]int32, k)
	for i := range palette {
		palette[i] = rng.Int31() - (1 << 30)
	}
	out := make([]int32, n)
	for i := range out {
		out[i] = palette[rng.Intn(k)]
	}
	return out
}

func TestSortUint64MainPath(t *testing.T) {
	data := buildLowCardU64(1_000_000, 1_000, 42)
	ref := append([]uint64(nil), data...)
	sort.Slice(ref, func(i, j int) bool { return ref[i] < ref[j] })

	Sort(data)

	for i := range data {
		if data[i] != ref[i] {
			t.Fatalf("mismatch at i=%d: got %d want %d", i, data[i], ref[i])
		}
	}
}

func TestSortUint64Tiny(t *testing.T) {
	data := buildLowCardU64(500_000, 4, 100)
	ref := append([]uint64(nil), data...)
	sort.Slice(ref, func(i, j int) bool { return ref[i] < ref[j] })

	Sort(data)

	for i := range data {
		if data[i] != ref[i] {
			t.Fatalf("mismatch at i=%d: got %d want %d", i, data[i], ref[i])
		}
	}
}

func TestSortInt64SignedSpan(t *testing.T) {
	data := buildLowCardI64(500_000, 500, 7)
	ref := append([]int64(nil), data...)
	sort.Slice(ref, func(i, j int) bool { return ref[i] < ref[j] })

	Sort(data)

	for i := range data {
		if data[i] != ref[i] {
			t.Fatalf("int64 mismatch at i=%d: got %d want %d", i, data[i], ref[i])
		}
	}
}

func TestSortInt32SignedSpan(t *testing.T) {
	data := buildLowCardI32(500_000, 500, 11)
	ref := append([]int32(nil), data...)
	sort.Slice(ref, func(i, j int) bool { return ref[i] < ref[j] })

	Sort(data)

	for i := range data {
		if data[i] != ref[i] {
			t.Fatalf("int32 mismatch at i=%d: got %d want %d", i, data[i], ref[i])
		}
	}
}

func TestEmptyAndSingle(t *testing.T) {
	var empty []uint64
	Sort(empty)
	if len(empty) != 0 {
		t.Fatal("empty slice changed")
	}
	single := []uint64{42}
	Sort(single)
	if single[0] != 42 {
		t.Fatal("single slice changed")
	}
}
