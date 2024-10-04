use core::slice;
use std::{ffi::CStr, ptr::NonNull};

/// Converts pointer to reference to str
///
/// # Panics
///
/// Functions panics if pointer is null or points to a non-valid UTF-8 data
///
pub fn ptr_to_str<'a>(ptr: *const i8) -> &'a str {
    unsafe { CStr::from_ptr(ptr).to_str().expect("valid UTF-8") }
}

/// Tries to convert pointer to reference to str
///
/// If pointer is not a valid UTF-8 data or if it's null, the None option is returned.
///
pub fn try_ptr_to_str<'a>(ptr: *const i8) -> Option<&'a str> {
    if ptr.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(ptr).to_str() }.ok()
}

/// Tries to copy data behind the pointer to a new object
///
///
pub fn try_ptr_to_type<T: Copy>(data: *const T) -> Option<T> {
    unsafe { data.as_ref() }.copied()
}

/// Converts pointer to a slice
///
/// If data is NULL, 0-length slice is returned
///
pub fn ptr_to_slice<'a, T>(data: *const T, len: u64) -> &'a [T] {
    let data = match len {
        0 => NonNull::dangling().as_ptr(),
        _ => data,
    };
    unsafe { slice::from_raw_parts(data, len as usize) }
}

/// Convert pointer to a slice
///
/// If data is NULL, a None option is returned. Otherwise slice is wrapped
/// in a Some variant.
///
pub fn try_ptr_to_slice<'a, T>(data: *const T, len: u64) -> Option<&'a [T]> {
    if data.is_null() {
        return None;
    }
    Some(ptr_to_slice(data, len))
}

/// Converts an integer value into a boolean
///
pub fn ptr_to_bool<T: Copy + PartialEq<i32>>(data: *const T) -> bool {
    try_ptr_to_type(data).map(|x| x != 0).unwrap_or(false)
}
