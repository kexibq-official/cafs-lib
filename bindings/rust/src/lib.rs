extern "C" {
    fn cafs_sort_u64(data: *mut u64, n: usize);
    fn cafs_sort_i64(data: *mut i64, n: usize);
    fn cafs_sort_i32(data: *mut i32, n: usize);
}

mod sealed {
    pub trait Sealed {}
    impl Sealed for u64 {}
    impl Sealed for i64 {}
    impl Sealed for i32 {}
}

pub trait Sortable: sealed::Sealed {
    #[doc(hidden)]
    fn __sort_impl(data: &mut [Self])
    where
        Self: Sized;
}

impl Sortable for u64 {
    #[inline]
    fn __sort_impl(data: &mut [u64]) {
        if data.len() < 2 {
            return;
        }
        unsafe { cafs_sort_u64(data.as_mut_ptr(), data.len()) }
    }
}

impl Sortable for i64 {
    #[inline]
    fn __sort_impl(data: &mut [i64]) {
        if data.len() < 2 {
            return;
        }
        unsafe { cafs_sort_i64(data.as_mut_ptr(), data.len()) }
    }
}

impl Sortable for i32 {
    #[inline]
    fn __sort_impl(data: &mut [i32]) {
        if data.len() < 2 {
            return;
        }
        unsafe { cafs_sort_i32(data.as_mut_ptr(), data.len()) }
    }
}

#[inline]
pub fn sort<T: Sortable>(data: &mut [T]) {
    T::__sort_impl(data)
}

#[cfg(test)]
mod tests {
    use super::sort;

    fn build_random_low_card<T: Copy>(n: usize, k: usize, seed: u64, mut to_t: impl FnMut(u64) -> T) -> Vec<T> {
        let mut state = seed.wrapping_add(0x9e3779b97f4a7c15);
        let mut next = || {
            state = state
                .wrapping_mul(6364136223846793005)
                .wrapping_add(1442695040888963407);
            state >> 1
        };
        let palette: Vec<T> = (0..k).map(|_| to_t(next())).collect();
        (0..n).map(|_| palette[(next() as usize) % k]).collect::<Vec<T>>()
    }

    #[test]
    fn matches_std_sort_u64() {
        for &(n, k) in &[(100_000usize, 4usize), (1_000_000, 1_000), (1_000_000, 100_000)] {
            let mut data = build_random_low_card(n, k, 42 + n as u64 + k as u64, |x| x);
            let mut reference = data.clone();
            reference.sort_unstable();
            sort(&mut data);
            assert_eq!(data, reference, "u64 mismatch at N={n}, K={k}");
        }
    }

    #[test]
    fn matches_std_sort_i64_signed_span() {
        let mut data: Vec<i64> = build_random_low_card(500_000, 500, 7, |x| x as i64);
        let mut reference = data.clone();
        reference.sort_unstable();
        sort(&mut data);
        assert_eq!(data, reference);
    }

    #[test]
    fn matches_std_sort_i32_signed_span() {
        let mut data: Vec<i32> = build_random_low_card(500_000, 500, 11, |x| x as i32);
        let mut reference = data.clone();
        reference.sort_unstable();
        sort(&mut data);
        assert_eq!(data, reference);
    }

    #[test]
    fn empty_and_single() {
        let mut empty: Vec<u64> = Vec::new();
        sort(&mut empty);
        let mut single: Vec<u64> = vec![42];
        sort(&mut single);
        assert_eq!(single, vec![42]);
    }
}
