// Keep the names of the original C++ code
#![allow(non_snake_case, non_upper_case_globals)]

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

#[aspis(to_harden)]
static mut ptrVector: Vec<*mut i32> = Vec::new();

fn main() {
    // Example of advanced container usage with dynamic memory
    let vec = unsafe { &mut *&raw mut ptrVector };
    vec.reserve(5); // reserve capacity for efficiency

    // Allocate integers on the heap and store pointers in the vector
    for i in 0..5 {
        vec.push(Box::into_raw(Box::new(i)));
    }

    // Output the elements pointed by the pointers in the vector
    for &p in vec.iter() {
        print!("{} ", unsafe { *p });
    }
    println!();

    // Note: We intentionally avoid freeing the allocated integers here.
    // Under ASPIS instrumentation, freeing them (either manually or via Box::from_raw)
    // could cause a double free (each allocation is tracked/duplicated).
    // The OS will reclaim this memory when the program exits.
}
