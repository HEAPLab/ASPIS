#include <iostream>

// ASPIS error handlers (non-duplicated)
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

// Allocate a single integer on the heap
__attribute__((annotate("to_harden")))
int *p = new int(5);

// A function that allocates memory, uses it, and then deletes it
int main() {
    *p = 10;  // modify the allocated value
    int result = *p;
    delete p;

    // Print the result (non-duplicated)
    std::cout << "Value: " << result << std::endl;
    return 0;
}
