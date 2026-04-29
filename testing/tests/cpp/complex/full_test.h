#pragma once

#include <iostream>
#include <string>
#include <stdexcept> 

#include <cmath>

// Simple function
void simpleFunction();
void simpleToHardenFunction();

float relAltitude(float pressure, float pressureRef, float temperatureRef);

// Function with parameters and return value
int add(int a, int b);

// Function overloading
int multiply(int a, int b);

double multiply(double a, double b);

// Sret
struct State
{
    double x0;
    float x1;
    float x2;
    float x3;
};

class Class
{
public:
    Class();

    State testSretDuplication();

    State state;
};

// Class with member functions and constructor
class MyClass {
public:
    MyClass(Class *c);

    MyClass(int x, int y);
    
    // Member function
    virtual int sum() const;

    bool start();
    
    // Virtual function for testing polymorphism
    virtual void print(Class &c) const;

    int a, b;
};

namespace Main {
// Derived class overriding a virtual function
class DerivedClass : public MyClass {
public:
    DerivedClass(int x, int y, int z);

    // Override virtual function
    void print(Class &c) const override;


private:
    int c;
    Class *bho;
};
} // namespace Main

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
int factorial(int n);

// Function that throws an exception
void riskyFunction(bool throwException);

// noexcept
void TestNoExcept() noexcept;

// testing unexpected special variable
static Class staticClass;
