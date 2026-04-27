#include <iostream>

extern "C"
void DataCorruption_Handler() {
    std::cerr << "ASPIS error: Data corruption detected\n";
    std::exit(EXIT_FAILURE);
}

extern "C"
void SigMismatch_Handler() {
    std::cerr << "ASPIS error: Signature mismatch detected\n";
    std::exit(EXIT_FAILURE);
}

// compute n! recursively
__attribute__((annotate("to_harden")))
unsigned long long fact(unsigned int n)
{
    return n <= 1 ? 1 : n * fact(n - 1);
}

int main()
{
    std::cout << fact(10) << std::endl;
    return 0;
}
