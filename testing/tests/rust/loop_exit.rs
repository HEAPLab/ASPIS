use aspis_annotations::aspis;

#[aspis(to_harden)]
pub static mut sum: i32 = 0;

fn DataCorruption_Handler() {
    println!("ASPIS_FAULT_INJECTION_CAUGHT: DataCorruption_Handler");
    std::io::stdout().flush().unwrap();
}

fn SigMismatch_Handler() {
    println!("ASPIS_FAULT_INJECTION_CAUGHT: SigMismatch_Handler");
    std::io::stdout().flush().unwrap();
}

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

    println!("{}", sum);
}

// expected output
// 2
