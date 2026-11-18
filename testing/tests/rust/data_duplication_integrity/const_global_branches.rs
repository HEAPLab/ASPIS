/*
This test verifies that a hardened function can store the address of a
constant global in one branch and pass the same global to a call in another
branch: the temporary duplicate that REDDI creates for the global at the store
must not be reused by the call, which it does not dominate.
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

static VALUES: [i32; 3] = [1, 2, 3];

fn sum(v: &[i32]) -> i32 {
    let mut s = 0;
    for x in v {
        s += x;
    }
    s
}

#[aspis(to_harden)]
fn pick(args: usize) -> i32 {
    let mut p: &[i32] = &[];
    let mut n = 0;
    // Branch on a value not known at compile time
    if args > 1 {
        n = sum(&VALUES);
    } else {
        p = &VALUES;
    }
    n + sum(p)
}

fn main() {
    println!("{}", pick(std::env::args().count()));
}

// expected output
// 6
