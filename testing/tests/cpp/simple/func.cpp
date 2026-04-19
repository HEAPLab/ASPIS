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

// Global variable used in the test
int g = 0;

// A function with a side effect on a global variable (non-duplicated)
__attribute__((annotate("exclude")))  // Marked as non-duplicable
void fNonDup() {
    g++;
    std::cout << "Function fNonDup called (g incremented to " << g << ")" << std::endl;
}

void f() {
    g++;
    std::cout << "Function f called (g incremented to " << g << ")" << std::endl;
}

int main() {
    f();  // Called once
    fNonDup();
    std::cout << "Final value of g: " << g << std::endl;
    return 0;
}
