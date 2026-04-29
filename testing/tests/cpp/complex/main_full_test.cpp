#include "full_test.h"

// Main::DerivedClass __attribute__((annotate("to_harden"))) *globalDClass;
Main::DerivedClass *globalDClass;
MyClass __attribute__((annotate("to_harden"))) *myClassGlobal;

void startClass(MyClass &c) {
    c.start();
}

int main() {
    printf("Starting main\n");
    Class toPassToMyClass;
    printf("After toPassToMyClass\n");
    std::cout << toPassToMyClass.state.x0 << " " << toPassToMyClass.state.x1 << " " << toPassToMyClass.state.x2 << " " << toPassToMyClass.state.x3 << "\n";
    printf("After toPassToMyClass\n");
    myClassGlobal = new MyClass(&toPassToMyClass);
    printf("Created myClassGlobal\n");
    myClassGlobal->print(toPassToMyClass);
    printf("printed\n");
    myClassGlobal->a = relAltitude(myClassGlobal->b, myClassGlobal->b - 160, 273.0f);
    printf("relAltituded\n");
    printf("myClassGlobal->a: %d\n", myClassGlobal->a);
    myClassGlobal->print(toPassToMyClass);
    printf("printed2\n");    

    globalDClass = new Main::DerivedClass(1,2,3);


    globalDClass->sum();
    globalDClass->start();
    
    startClass(*globalDClass);
    
    // Test sret
    Class sretTest;
    std::cout << "test sret: " << sretTest.testSretDuplication().x0 << std::endl;

    std::cout << "staticClass: " << staticClass.testSretDuplication().x3 << std::endl;

    // Call simple function
    simpleFunction();
    
    // Call function with parameters
    std::cout << "Add 3 + 4 = " << add(3, 4) << std::endl;
    
    // Call overloaded functions
    std::cout << "Multiply 3 * 4 = " << multiply(3, 4) << std::endl;
    std::cout << "Multiply 2.5 * 4.5 = " << multiply(2.5, 4.5) << std::endl;

    // Test class and member function
    MyClass myObj(5, 7);
    std::cout << "Sum of MyClass: " << myObj.sum() << std::endl;
    myObj.print(sretTest);
    
    // Test derived class with overridden virtual function
    Main::DerivedClass derivedObj(3, 6, 9);
    derivedObj.print(sretTest);

    // Test template function
    std::cout << "Square of 5: " << square(5) << std::endl;
    std::cout << "Square of 2.5: " << square(2.5) << std::endl;

    // Test template class
    TemplateClass<int> intObj(3, 4);
    TemplateClass<double> doubleObj(2.5, 3.5);
    std::cout << "Product of intObj: " << intObj.product() << std::endl;
    std::cout << "Product of doubleObj: " << doubleObj.product() << std::endl;

    // Test recursive function
    std::cout << "Factorial of 5: " << factorial(5) << std::endl;

    // Test exception handling
    try {
        riskyFunction(true);  // This will throw an exception
    } catch (const std::runtime_error& e) {
        std::cerr << "Caught exception: " << e.what() << std::endl;
    }

    try {
        riskyFunction(false); // This will not throw an exception
    } catch (const std::runtime_error& e) {
        std::cerr << "Caught exception: " << e.what() << std::endl;
    }
    
    TestNoExcept();

    return 0;
}