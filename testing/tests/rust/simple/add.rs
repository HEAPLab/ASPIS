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
static mut A: i32 = 10;

fn main() {
    let b = 20;
    let c = unsafe { A } + b;
    println!("{}", c);
}
