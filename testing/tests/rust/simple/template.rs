// Keep the names of the original C++ code
#![allow(non_snake_case, non_upper_case_globals)]

use std::ops::AddAssign;

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

// Print function (non-duplicated)
#[aspis(exclude)]
#[unsafe(link_section = ".text.hot")]
fn printResult(value: i32) {
    println!("Result: {}", value);
}

// Example of a generic function
fn myMax<T: PartialOrd>(a: T, b: T) -> T {
    return if a > b { a } else { b };
}

// Default::default() cannot be called in a static initializer,
// so the C++ value-initialization T() is replaced by this constant.
trait Zero {
    const ZERO: Self;
}

impl Zero for i32 {
    const ZERO: Self = 0;
}

// Example of a simple generic accumulator struct
struct Accumulator<T> {
    sum: T,
}

impl<T: Zero + AddAssign + Copy> Accumulator<T> {
    const fn new() -> Self {
        Accumulator { sum: T::ZERO }
    }

    fn add(&mut self, value: T) {
        self.sum += value; // duplicated safely
    }

    fn total(&self) -> T {
        return self.sum;
    }
}

#[aspis(to_harden)]
static mut acc: Accumulator<i32> = Accumulator::new();

fn main() {
    let x = 42;
    let y = 17;
    let maxVal = myMax::<i32>(x, y); // 42

    let sumVal;
    unsafe {
        (*&raw mut acc).add(5);
        (*&raw mut acc).add(10);
        sumVal = (*&raw const acc).total(); // 15
    }

    printResult(maxVal);
    printResult(sumVal);
}
