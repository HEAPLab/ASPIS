// Keep the names of the original C++ code
#![allow(non_snake_case, non_upper_case_globals)]

mod full_test;

use aspis_rust_annotations::aspis;

use full_test::*;

// #[aspis(to_harden)]
static mut globalDClass: *mut Main::DerivedClass = std::ptr::null_mut();
#[aspis(to_harden)]
static mut myClassGlobal: *mut MyClass = std::ptr::null_mut();

fn startClass(c: &dyn MyClassVirtual) {
    c.start();
}

fn main() {
    println!("Starting main");
    let toPassToMyClass = Class::new();
    println!("After toPassToMyClass");
    println!(
        "{} {} {} {}",
        toPassToMyClass.state.x0,
        toPassToMyClass.state.x1,
        toPassToMyClass.state.x2,
        toPassToMyClass.state.x3
    );
    println!("After toPassToMyClass");
    unsafe {
        myClassGlobal = Box::into_raw(Box::new(MyClass::from_class(&toPassToMyClass)));
        println!("Created myClassGlobal");
        (*myClassGlobal).print(&toPassToMyClass);
        println!("printed");
        (*myClassGlobal).a = relAltitude(
            (*myClassGlobal).b as f32,
            ((*myClassGlobal).b - 160) as f32,
            273.0,
        ) as i32;
        println!("relAltituded");
        println!("myClassGlobal->a: {}", (*myClassGlobal).a);
        (*myClassGlobal).print(&toPassToMyClass);
        println!("printed2");

        globalDClass = Box::into_raw(Box::new(Main::DerivedClass::new(1, 2, 3)));

        (*globalDClass).sum();
        (*globalDClass).start();

        startClass(&*globalDClass);
    }

    // Test sret
    let sretTest = Class::new();
    println!("test sret: {}", sretTest.testSretDuplication().x0);

    println!("staticClass: {}", staticClass.testSretDuplication().x3);

    // Call simple function
    simpleFunction();

    // Call function with parameters
    println!("Add 3 + 4 = {}", add(3, 4));

    // Call overloaded functions
    println!("Multiply 3 * 4 = {}", multiply(3, 4));
    println!("Multiply 2.5 * 4.5 = {}", multiply(2.5, 4.5));

    // Test class and member function
    let myObj = MyClass::new(5, 7);
    println!("Sum of MyClass: {}", myObj.sum());
    myObj.print(&sretTest);

    // Test derived class with overridden virtual function
    let derivedObj = Main::DerivedClass::new(3, 6, 9);
    derivedObj.print(&sretTest);

    // Test template function
    println!("Square of 5: {}", square(5));
    println!("Square of 2.5: {}", square(2.5));

    // Test template class
    let intObj = TemplateClass::<i32>::new(3, 4);
    let doubleObj = TemplateClass::<f64>::new(2.5, 3.5);
    println!("Product of intObj: {}", intObj.product());
    println!("Product of doubleObj: {}", doubleObj.product());

    // Test recursive function
    println!("Factorial of 5: {}", factorial(5));

    // Test exception handling
    if let Err(e) = riskyFunction(true) {
        // This will return an error
        eprintln!("Caught exception: {}", e);
    }

    if let Err(e) = riskyFunction(false) {
        // This will not return an error
        eprintln!("Caught exception: {}", e);
    }

    TestNoExcept();
}
