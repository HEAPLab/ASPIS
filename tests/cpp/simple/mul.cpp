#include <iostream>

int multiply(int a, int b)
{
    return a * b;
}

int main()
{
    int result = multiply(6, 7);
    std::cout << result << std::endl;
    return 0;
}
