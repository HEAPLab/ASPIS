use aspis_rust_annotations::aspis;

#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    eprintln!("Errore ASPIS: Data corruption detected");
    std::process::exit(-1);
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    eprintln!("Errore ASPIS: Signature mismatch detected");
    std::process::exit(-1);
}

static mut G: i32 = 0;

#[aspis(exclude)]
#[unsafe(link_section = ".text.hot")]
fn f_non_dup() {
    unsafe {
        G += 1;
    }
    let copy = unsafe { G };
    println!("Function f_non_dup called (G incremented to {})", copy);
}

#[aspis(to_harden)]
#[unsafe(link_section = ".text.hot")]
fn f() {
    unsafe {
        G += 1;
    }
    let copy = unsafe { G };
    println!("Function f called (G incremented to {})", copy);
}

fn main() {
    f(); // Called once
    f_non_dup();
    let copy = unsafe { G };
    println!("Final value of G: {}", copy);
}
