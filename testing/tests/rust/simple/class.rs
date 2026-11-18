// Keep the names of the original C++ code
#![allow(non_snake_case, non_upper_case_globals)]

use aspis_rust_annotations::aspis;

// Gestori di errore ASPIS
#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    eprintln!("Errore ASPIS: Data corruption detected");
    std::process::exit(1);
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    eprintln!("Errore ASPIS: Signature mismatch detected");
    std::process::exit(1);
}

trait Print {
    fn print(&self);
}

// Class with member functions and constructor
struct MyClass {
    a: i32,
    b: i32,
}

impl MyClass {
    const fn new(x: i32, y: i32) -> Self {
        MyClass { a: x, b: y }
    }

    // Member function
    fn sum(&self) -> i32 {
        return self.a + self.b;
    }
}

impl Print for MyClass {
    fn print(&self) {
        println!("{}, {}", self.a, self.b);
    }
}

struct DerivedClass {
    base: MyClass,
    c: i32,
}

impl DerivedClass {
    const fn new(x: i32, y: i32, z: i32) -> Self {
        DerivedClass {
            base: MyClass::new(x, y),
            c: z,
        }
    }
}

// Override virtual function
impl Print for DerivedClass {
    fn print(&self) {
        println!("{}, {}, {}", self.base.a, self.base.b, self.c);
    }
}

#[aspis(to_harden)]
static mut derivedObj: DerivedClass = DerivedClass::new(3, 6, 9);

fn main() {
    // Test class and member function
    let myObj = MyClass::new(5, 7);
    println!("{}", myObj.sum());
    myObj.print();

    unsafe { (*&raw const derivedObj).print() };
}
