#include <iostream>

int main()
{
    int x = 10;
    int y = 5;

    auto by_value = [x]()
    {
        std::cout << x << "\n";
    };

    auto by_ref = [&y]()
    {
        std::cout << y << "\n";
    };

    by_value(); // Should print 10
    by_ref();   // Should print 5

    y = 15;
    by_ref(); // Should print 15

    return 0;
}
