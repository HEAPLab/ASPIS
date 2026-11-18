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

// compute n! recursively
#[aspis(to_harden)]
#[unsafe(link_section = ".text.hot")]
fn fact(n: u32) -> u64 {
    if n <= 1 {
        1
    } else {
        n as u64 * fact(n - 1)
    }
}

fn main() {
    let result = fact(10);
    println!("{}", result);
}
