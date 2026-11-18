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
pub static mut sum: i32 = 0;

fn main() {
    let mut i = 0;
    while i < 5 {
        if i == 1 {
            i += 1;
            continue;
        }
        if i == 3 {
            break;
        }
        unsafe {
            sum += i;
        }
        i += 1;
    }
    unsafe {
        let copy = sum;
        println!("{}", copy);
    }
}
