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
#[unsafe(link_section = ".text.hot")]
fn printResult(value: i32) {
    println!("{}", value);
}

// Inline function example (calculates square)
#[inline]
fn square(x: i32) -> i32 {
    return x * x;
}

// const fn example
const fn getFive() -> i32 {
    return 5;
}

fn main() {
    let a = 3;
    let b = 4;

    let result1 = square(a); // 9
    let result2 = square(b); // 16
    const c: i32 = getFive(); // 5

    printResult(result1);
    printResult(result2);
    printResult(c);
}
