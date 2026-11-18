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

#[aspis(to_harden)]
#[unsafe(link_section = ".data.hot")]
#[unsafe(no_mangle)]
pub static mut a: i32 = 10;

fn main() {
    let b = 20;
    if unsafe { a } < b {
        print!("OK");
    } else {
        print!("FAIL");
    }
}

// expected output
// OK
