#include <iostream>

volatile int flag = 0;

void writer()
{
    flag = 42;
}

void reader()
{
    int local = flag;
    std::cout << local << "\n";
}

int main()
{
    writer(); // Writes 42 to the volatile variable
    reader(); // Reads it back and prints it
    return 0;
}
