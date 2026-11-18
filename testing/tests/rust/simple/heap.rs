// Keep the names of the original C++ code
#![allow(non_upper_case_globals)]

use aspis_rust_annotations::aspis;

// ASPIS error handlers (non-duplicated)
#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    eprintln!("Errore ASPIS: Data corruption detected");
    std::process::exit(1);
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    eprintln!("Errore ASPIS: Signature mismatch detected");
    std::process::exit(1);
}

// Allocate a single integer on the heap.
// Rust statics cannot be initialized with a heap allocation, so the
// allocation happens at the start of main instead of before it.
#[aspis(to_harden)]
static mut p: *mut i32 = std::ptr::null_mut();

// A function that allocates memory, uses it, and then deletes it
fn main() {
    let result: i32;
    unsafe {
        p = Box::into_raw(Box::new(5));

        *p = 10; // modify the allocated value
        result = *p;
        drop(Box::from_raw(p));
    }

    // Print the result (non-duplicated)
    println!("Value: {}", result);
}
