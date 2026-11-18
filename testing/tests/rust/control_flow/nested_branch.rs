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
    for i in 0..3 {
        let x = i * 2;
        if x % 2 == 0 {
            unsafe {
                sum += x;
            }
        } else {
            unsafe {
                sum -= x;
            }
        }
    }
    unsafe {
        let copy = sum;
        print!("{}", copy);
    }
}

// expected output
// 6
