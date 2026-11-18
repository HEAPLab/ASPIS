// Keep the names of the original C++ code
#![allow(non_snake_case)]

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

// Executes a function/closure in a non-duplicated context
#[aspis(exclude)] // Marked as non-duplicable
#[unsafe(link_section = ".text.hot")]
fn runNoDup<F: Fn()>(func: F) {
    func();
}

#[aspis(to_harden)]
#[unsafe(link_section = ".text.hot")]
fn run<F: Fn()>(func: F) {
    func();
}

fn main() {
    // Example 1: closure capturing a local variable by reference
    let mut x = 0;
    let mut incrX = |val: i32| {
        x += val;
        println!("x incremented by {}", val);
    };
    incrX(5); // can be duplicated safely (x is local to each duplicate)

    // Example 2: closure capturing a heap-allocated pointer
    let p: *mut i32 = Box::into_raw(Box::new(10));
    let incPtr = move || unsafe {
        *p += 1;
    };
    runNoDup(incPtr); // non-duplicated increment of shared memory

    run(incPtr); // duplicated increment of shared memory

    println!("Value pointed by p: {}", unsafe { *p });
    drop(unsafe { Box::from_raw(p) });
}
