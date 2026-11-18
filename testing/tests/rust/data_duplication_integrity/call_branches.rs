/*
This test verifies that a non-hardened function can pass the same local
variable to a hardened function from two different branches: the temporary
duplicate that REDDI creates for the argument of one call must not be reused
by the other call, which it does not dominate.
*/

use aspis_rust_annotations::aspis;

// ASPIS error handlers (non-duplicated)
#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    eprintln!("ASPIS error: data corruption detected");
    std::process::exit(1);
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    eprintln!("ASPIS error: signature mismatch detected");
    std::process::exit(1);
}

#[aspis(to_harden)]
fn add_one(x: &mut i32) {
    *x += 1;
}

fn main() {
    let mut x = 1;
    // Branch on a value not known at compile time
    if std::env::args().count() > 1 {
        add_one(&mut x);
    } else {
        add_one(&mut x);
        add_one(&mut x);
    }
    println!("{}", x);
}

// expected output
// 3
