#include <iostream>

// ASPIS error handlers (non-duplicated)
__attribute__((no_duplicate))
void DataCorruption_Handler() {
    std::cerr << "ASPIS error: Data corruption detected\n";
}

__attribute__((no_duplicate))
void SigMismatch_Handler() {
    std::cerr << "ASPIS error: Signature mismatch detected\n";
}

// Global variable used in the test
int g = 0;

// A function with a side effect on a global variable (non-duplicated)
__attribute__((no_duplicate))
void f() {
    g++;
    std::cout << "Function f called (g incremented to " << g << ")" << std::endl;
}

int main() {
    f();  // Called once
    std::cout << "Final value of g: " << g << std::endl;
    return 0;
}
