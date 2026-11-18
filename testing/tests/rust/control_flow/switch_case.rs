/*
 * This test verifies that ASPIS correctly instruments control-flow
 * integrity for indirect jumps produced by a match structure.
 */

#![allow(non_upper_case_globals)]

use aspis_rust_annotations::aspis;
use std::io::Write;

#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    println!("ASPIS_FAULT_INJECTION_CAUGHT: DataCorruption_Handler");
    std::io::stdout().flush().unwrap();
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    println!("ASPIS_FAULT_INJECTION_CAUGHT: SigMismatch_Handler");
    std::io::stdout().flush().unwrap();
}

fn switch_test(value: i32) -> i32 {
    match value {
        0 => 100,
        1 => 200,
        2 => 250,
        3 => 300,
        4 => 400,
        _ => -1,
    }
}

#[aspis(to_harden)]
#[unsafe(link_section = ".data.hot")]
#[unsafe(no_mangle)]
pub static mut switchN: i32 = 3;

fn main() {
    let result = switch_test(unsafe { switchN });
    print!("{}", result);
}

// expected output
// 300
