#include <iostream>

// Class with member functions and constructor
class MyClass
{
public:
    MyClass(int x, int y) : a(x), b(y) {}

    // Member function
    int sum() const
    {
        return a + b;
    }

    // Virtual function for testing polymorphism
    virtual void print() const
    {
        std::cout << a << ", " << b << std::endl;
    }

protected:
    int a, b;
};

// Derived class overriding a virtual function
class DerivedClass : public MyClass
{
public:
    DerivedClass(int x, int y, int z) : MyClass(x, y), c(z) {}

    // Override virtual function
    void print() const override
    {
        std::cout << a << ", " << b << ", " << c << std::endl;
    }

private:
    int c;
};

int main()
{
    // Test class and member function
    MyClass myObj(5, 7);
    std::cout << myObj.sum() << std::endl;
    myObj.print();

    // Test derived class with overridden virtual function
    DerivedClass derivedObj(3, 6, 9);
    derivedObj.print();
}