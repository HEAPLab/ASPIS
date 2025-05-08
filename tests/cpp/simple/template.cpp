#include <iostream>

// Template function
template <typename T>
T square(T x)
{
    return x * x;
}

// Template class
template <typename T>
class TemplateClass
{
public:
    TemplateClass(T x, T y) : a(x), b(y) {}

    T product() const
    {
        return a * b;
    }

private:
    T a, b;
};

int main()
{
    // Test template function
    std::cout << square(5) << std::endl;

    // Test template class
    TemplateClass<int> obj(3, 4);
    std::cout << obj.product() << std::endl;

    return 0;
}
