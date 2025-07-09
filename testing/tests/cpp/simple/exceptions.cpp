#include <iostream>
#include <cstdlib>
#include <stdexcept>

// ASPIS error handlers (non-duplicated)
__attribute__((no_duplicate))
void DataCorruption_Handler() {
    std::cerr << "ASPIS error: Data corruption detected\n";
    std::exit(1);
}

__attribute__((no_duplicate))
void SigMismatch_Handler() {
    std::cerr << "ASPIS error: Signature mismatch detected\n";
    std::exit(1);
}

// A leaf that always throws
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
