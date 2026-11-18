// Keep the names of the original C++ code
#![allow(non_snake_case, unused_variables, dead_code)]

use aspis_rust_annotations::aspis;

// ASPIS error handlers (non-duplicated)
#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    eprintln!("Errore ASPIS: Data corruption detected");
    std::process::exit(-1);
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    eprintln!("Errore ASPIS: Signature mismatch detected");
    std::process::exit(-1);
}

// Helper to print two pointer values (non-duplicated)
#[aspis(to_harden)]
#[unsafe(link_section = ".text.hot")]
fn printPointers(p1: *mut i32, p2: *mut i32) {
    unsafe {
        print!("Value pointed by p1: {}", *p1);
        if !p2.is_null() {
            print!(", Value pointed by p2: {}", *p2);
        }
    }
    println!();
}

fn main() {
    // Allocate a small array on the heap
    let buffer: *mut i32 = Box::into_raw(Box::new([0i32; 2])).cast();
    unsafe {
        *buffer.add(0) = 100;
        *buffer.add(1) = 200;
    }
    // Use a single pointer + offset instead of two aliases
    let base: *mut i32 = buffer;
    let secondValue: i32 = unsafe { *base.add(1) };

    // Example struct with two distinct heap allocations
    struct Pair {
        a: i32,
        b: i32,
    }
    let obj: *mut Pair = Box::into_raw(Box::new(Pair { a: 1, b: 2 }));

    // Example of copying data instead of aliasing
    let p1: *mut i32 = Box::into_raw(Box::new(42));
    // let p2: *mut i32 = p1;  // removed aliasing
    let p2: *mut i32 = Box::into_raw(Box::new(unsafe { *p1 })); // copy the value

    // Print the results (non-duplicated)
    printPointers(p1, p2);

    unsafe {
        drop(Box::from_raw(buffer.cast::<[i32; 2]>()));
        drop(Box::from_raw(obj));
        drop(Box::from_raw(p1));
        drop(Box::from_raw(p2));
    }
}
