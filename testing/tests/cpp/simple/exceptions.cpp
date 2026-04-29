#include <iostream>
#include <cstdlib>
#include <stdexcept>

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

// A leaf that always throws
__attribute__((annotate("to_harden")))
int nested_thrower() {
    throw std::runtime_error("Test exception");
}

// A function that calls nested_thrower and catches
void nested_catcher() {
    try {
        nested_thrower();
    } catch (...) {
        // Report that we caught an exception
        std::cout << "Exception caught: Test exception" << std::endl;
    }
}

int main() {
    // Call nested_catcher twice to demonstrate
    nested_catcher();
    nested_catcher();
    return 0;
}
