#include <iostream>

// ASPIS error handlers (never duplicated)
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

__attribute__((annotate("to_harden")))
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
