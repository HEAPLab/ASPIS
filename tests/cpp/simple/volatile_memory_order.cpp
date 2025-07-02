#include <iostream>

// ASPIS error handlers (never duplicated)
void DataCorruption_Handler() {
    std::cerr << "Data corruption detected\n";
    std::exit(1);
}

void SigMismatch_Handler() {
    std::cerr << "Signature mismatch detected\n";
    std::exit(1);
}


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
