use aspis_rust_annotations::aspis;
use std::io::Write;

#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    eprintln!("ASPIS_FAULT_INJECTION_CAUGHT: DataCorruption_Handler");
    std::process::exit(-1);
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    eprintln!("ASPIS_FAULT_INJECTION_CAUGHT: SigMismatch_Handler");
    std::process::exit(-1);
}

#[aspis(to_harden)]
#[unsafe(link_section = ".text.hot")]
fn multiply(a: i32, b: i32) -> i32 {
    a * b
}

fn main() {
    let result = multiply(6, 7);
    println!("{}", result);
}
