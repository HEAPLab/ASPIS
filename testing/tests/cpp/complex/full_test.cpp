#include <iostream>
#include <string>
#include <stdexcept> 
#include "full_test.h"

void DataCorruption_Handler()
{
    printf("DataCorruption_Handler\n");
    while(1);
}

void SigMismatch_Handler()
{
    printf("SigMismatch_Handler\n");
    while(1);
}

float relAltitude(float pressure, float pressureRef, float temperatureRef)
{
    return temperatureRef / 0.0065f * (1 - std::powf(pressure / pressureRef, 0.19026119f));
}

// testing unexpected special variable
static Class staticClassToHarden;
// static Class __attribute__((annotate("to_harden"))) staticClassToHarden;

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
MyClass::MyClass(int x, int y) : a(x), b(y) {}

MyClass::MyClass(Class *c) {
    printf("Start of constructor\n");
    std::cout << "x1: " << c->state.x1 << "\n";
    std::cout << "x2: " << c->state.x2 << "\n";
    a = c->state.x1;
    std::cout << "a: " << a << "\n";
    b = c->state.x2;
    std::cout << "b: " << b << "\n";
}

// Member function
int MyClass::sum() const {
    return a + b;
}

// Virtual function for testing polymorphism
void MyClass::print(Class &c) const {
    std::cout << "MyClass: a = " << a << ", b = " << b << std::endl;
}


bool MyClass::start() {
    Class c;
    print(c);

    return true;
}

// Derived class overriding a virtual function
Main::DerivedClass::DerivedClass(int x, int y, int z) : MyClass(x, y), c(z) {
    bho = new Class();
}

// Override virtual function
void Main::DerivedClass::print(Class &cl) const {
    std::cout << "DerivedClass: a = " << a << ", b = " << b << ", c = " << c << std::endl;
}

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

// Sret
Class::Class() {
    printf("Class constructor\n");
    state = {1.0, 2.f, 3.f, 4.f};
    if(false) {
        throw 1;
    }
}

State Class::testSretDuplication(){
    printf("testSretDuplication\n");
    bool callRecursive = (this != &staticClassToHarden);
    printf("Condition calculated: %d\n", callRecursive);
    if(callRecursive) {
        printf("Calling recursive testSretDuplication\n");
        staticClassToHarden.testSretDuplication();
    }
    return state;
}

// noexcept
void TestNoExcept() noexcept
{
    volatile MyClass m_data(1,2);
}

// void simpleToHardenFunction() __attribute__((annotate("to_harden"))) {
void simpleToHardenFunction() {
    simpleFunction();
}
