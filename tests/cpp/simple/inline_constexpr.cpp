#include <iostream>

class Math
{
public:
    static inline constexpr int Factor = 10;

    static constexpr int multiply(int value)
    {
        return value * Factor;
    }
};

int main()
{
    constexpr int result = Math::multiply(4);
    std::cout << result << "\n";
    return 0;
}
