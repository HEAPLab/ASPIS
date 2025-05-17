#include <iostream>

// compute n! recursively
unsigned long long fact(unsigned int n)
{
    return n <= 1 ? 1 : n * fact(n - 1);
}

int main()
{
    std::cout << fact(10) << std::endl;
    return 0;
}
