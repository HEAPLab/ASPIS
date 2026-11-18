// Keep the names of the original C++ code
#![allow(non_upper_case_globals)]

use std::ptr;

use aspis_rust_annotations::aspis;

// ASPIS error handlers (never duplicated)
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

// Rust has no volatile qualifier: every access to flag goes through
// ptr::read_volatile/ptr::write_volatile instead.
#[aspis(to_harden)]
static mut flag: i32 = 0;

fn writer() {
    unsafe { ptr::write_volatile(&raw mut flag, 42) };
}

fn reader() {
    let local = unsafe { ptr::read_volatile(&raw const flag) };
    println!("{}", local);
}

fn main() {
    writer(); // Writes 42 to the volatile variable
    reader(); // Reads it back and prints it
}
