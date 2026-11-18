// Rust port of full_test.h and full_test.cpp: the declarations of the header
// and their definitions live together in this module.

use std::ops::Mul;
use std::ptr;

// ASPIS error handlers
#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    println!("DataCorruption_Handler");
    loop {}
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    println!("SigMismatch_Handler");
    loop {}
}

pub fn relAltitude(pressure: f32, pressureRef: f32, temperatureRef: f32) -> f32 {
    return temperatureRef / 0.0065 * (1.0 - (pressure / pressureRef).powf(0.19026119));
}

// Sret
#[derive(Clone, Copy)]
pub struct State {
    pub x0: f64,
    pub x1: f32,
    pub x2: f32,
    pub x3: f32,
}

const INITIAL_STATE: State = State {
    x0: 1.0,
    x1: 2.0,
    x2: 3.0,
    x3: 4.0,
};

pub struct Class {
    pub state: State,
}

impl Class {
    pub fn new() -> Self {
        println!("Class constructor");
        Class {
            state: INITIAL_STATE,
        }
    }

    // Rust statics are initialized at compile time, so they cannot run the
    // printing constructor like the C++ static objects do before main.
    const fn new_static() -> Self {
        Class {
            state: INITIAL_STATE,
        }
    }

    pub fn testSretDuplication(&self) -> State {
        println!("testSretDuplication");
        let callRecursive = !ptr::eq(self, &staticClassToHarden);
        println!("Condition calculated: {}", callRecursive as i32);
        if callRecursive {
            println!("Calling recursive testSretDuplication");
            staticClassToHarden.testSretDuplication();
        }
        return self.state;
    }
}

// testing unexpected special variable
static staticClassToHarden: Class = Class::new_static();
// #[aspis(to_harden)]
// static staticClassToHarden: Class = Class::new_static();

// testing unexpected special variable
pub static staticClass: Class = Class::new_static();

// Simple function
pub fn simpleFunction() {
    println!("Simple function");
}

// Function with parameters and return value
pub fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// Rust has no function overloading: the C++ overloads become the
// implementations of a trait, selected by the type of the arguments.
pub trait Multiply {
    fn multiply(self, other: Self) -> Self;
}

impl Multiply for i32 {
    fn multiply(self, other: Self) -> Self {
        return self * other;
    }
}

impl Multiply for f64 {
    fn multiply(self, other: Self) -> Self {
        return self * other;
    }
}

pub fn multiply<T: Multiply>(a: T, b: T) -> T {
    return a.multiply(b);
}

// Class with member functions and constructor
pub struct MyClass {
    pub a: i32,
    pub b: i32,
}

impl MyClass {
    pub fn new(x: i32, y: i32) -> Self {
        MyClass { a: x, b: y }
    }

    pub fn from_class(c: &Class) -> Self {
        println!("Start of constructor");
        println!("x1: {}", c.state.x1);
        println!("x2: {}", c.state.x2);
        let a = c.state.x1 as i32;
        println!("a: {}", a);
        let b = c.state.x2 as i32;
        println!("b: {}", b);
        MyClass { a, b }
    }
}

// The virtual functions of MyClass, overridable by derived classes
pub trait MyClassVirtual {
    fn base(&self) -> &MyClass;

    // Member function
    fn sum(&self) -> i32 {
        return self.base().a + self.base().b;
    }

    // Virtual function for testing polymorphism
    fn print(&self, _c: &Class) {
        println!("MyClass: a = {}, b = {}", self.base().a, self.base().b);
    }

    fn start(&self) -> bool {
        let c = Class::new();
        self.print(&c);

        return true;
    }
}

impl MyClassVirtual for MyClass {
    fn base(&self) -> &MyClass {
        self
    }
}

pub mod Main {
    use super::{Class, MyClass, MyClassVirtual};

    // Derived class overriding a virtual function
    pub struct DerivedClass {
        base: MyClass,
        c: i32,
        #[allow(dead_code)]
        bho: *mut Class,
    }

    impl DerivedClass {
        pub fn new(x: i32, y: i32, z: i32) -> Self {
            DerivedClass {
                base: MyClass::new(x, y),
                c: z,
                bho: Box::into_raw(Box::new(Class::new())),
            }
        }
    }

    impl MyClassVirtual for DerivedClass {
        fn base(&self) -> &MyClass {
            &self.base
        }

        // Override virtual function
        fn print(&self, _cl: &Class) {
            println!(
                "DerivedClass: a = {}, b = {}, c = {}",
                self.base.a, self.base.b, self.c
            );
        }
    }
}

// Template function
pub fn square<T: Mul<Output = T> + Copy>(x: T) -> T {
    return x * x;
}

// Template class
pub struct TemplateClass<T> {
    a: T,
    b: T,
}

impl<T: Mul<Output = T> + Copy> TemplateClass<T> {
    pub fn new(x: T, y: T) -> Self {
        TemplateClass { a: x, b: y }
    }

    pub fn product(&self) -> T {
        return self.a * self.b;
    }
}

// Recursive function
pub fn factorial(n: i32) -> i32 {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
}

// Function that "throws an exception": the tests are built with
// -C panic=abort, so the error is returned instead of unwinding.
pub fn riskyFunction(throwException: bool) -> Result<(), String> {
    if throwException {
        return Err(String::from("An error occurred in riskyFunction!"));
    }
    println!("RiskyFunction executed successfully.");
    return Ok(());
}

// noexcept (Rust has no equivalent: functions are not expected to unwind here)
pub fn TestNoExcept() {
    // black_box stands in for the volatile qualifier of the C++ local
    let m_data = MyClass::new(1, 2);
    std::hint::black_box(&m_data);
}

// #[aspis(to_harden)]
#[allow(dead_code)]
pub fn simpleToHardenFunction() {
    simpleFunction();
}
