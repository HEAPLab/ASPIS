#include <iostream>
#include <cstdlib>

extern "C" {
    // ASPIS error handling functions
    void DataCorruption_Handler() {
        std::cerr << "Errore ASPIS: Data corruption detected\n";
        std::exit(EXIT_FAILURE);
    }

    void SigMismatch_Handler() {
        std::cerr << "Errore ASPIS: Signature mismatch detected\n";
        std::exit(EXIT_FAILURE);
    }
}

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
