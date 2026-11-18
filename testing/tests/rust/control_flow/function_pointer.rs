/*
 * Function pointer calls to validate inter-RASM control-flow checking.
 * ASPIS should correctly handle duplicated calls via function pointers.
 */

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

fn foo() -> i32 {
    return 42;
}

#[aspis(to_harden)]
#[unsafe(link_section = ".text.hot")]
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// rustc turns calls through a known function pointer into direct calls, even
// at opt-level=0: black_box hides the pointer so that the calls stay indirect.
fn main() {
    // Simple function pointer call
    let fptr: fn() -> i32 = std::hint::black_box(foo);
    let result = fptr();
    println!("{}", result);

    // Function pointer call with parameters
    let addptr: fn(i32, i32) -> i32 = std::hint::black_box(add);
    let sum = addptr(27, result);
    println!("{}", sum);
}
