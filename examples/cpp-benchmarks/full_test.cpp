#include <iostream>
#include <string>
#include <stdexcept> 

void DataCorruption_Handler()
{
    while(1);
}

void SigMismatch_Handler()
{
    while(1);
}

// Simple function
void simpleFunction() {
    std::cout << "Simple function" << std::endl;
}

// Function with parameters and return value
int add(int a, int b) {
    return a + b;
}

// Function overloading
int multiply(int a, int b) {
    return a * b;
}

double multiply(double a, double b) {
    return a * b;
}

// Class with member functions and constructor
class MyClass {
public:
    MyClass(int x, int y) : a(x), b(y) {}
    
    // Member function
    int sum() const {
        return a + b;
    }
    
    // Virtual function for testing polymorphism
    virtual void print() const {
        std::cout << "MyClass: a = " << a << ", b = " << b << std::endl;
    }

protected:
    int a, b;
};

// Derived class overriding a virtual function
class DerivedClass : public MyClass {
public:
    DerivedClass(int x, int y, int z) : MyClass(x, y), c(z) {}

    // Override virtual function
    void print() const override {
        std::cout << "DerivedClass: a = " << a << ", b = " << b << ", c = " << c << std::endl;
    }

private:
    int c;
};

// Template function
template <typename T>
T square(T x) {
    return x * x;
}

// Template class
template <typename T>
class TemplateClass {
public:
    TemplateClass(T x, T y) : a(x), b(y) {}
    
    T product() const {
        return a * b;
    }

private:
    T a, b;
};

// Recursive function
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

// Function that throws an exception
void riskyFunction(bool throwException) {
    if (throwException) {
        throw std::runtime_error("An error occurred in riskyFunction!");
    }
    std::cout << "RiskyFunction executed successfully." << std::endl;
}

int main() {
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
    myObj.print();
    
    // Test derived class with overridden virtual function
    DerivedClass derivedObj(3, 6, 9);
    derivedObj.print();

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

    return 0;
}
