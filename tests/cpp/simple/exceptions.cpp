#include <iostream>
#include <stdexcept>

// A leaf that always throws
[[noreturn]] void thrower()
{
    throw std::runtime_error("err");
}

inline void nested_catcher(int &count)
{
    try
    {
        try
        {
            thrower();
        }
        catch (const std::runtime_error &e)
        {
            ++count;
            throw;
        }
    }
    catch (...)
    {
        ++count;
    }
}

int main()
{
    int count = 0;
    for (int i = 0; i < 100000; ++i)
    {
        nested_catcher(count);
    }
    std::cout << count << "\n";
    return 0;
}
