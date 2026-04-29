#include <iostream>

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
int add(int a, int b) {
    return a + b;
}

int main() {
  std::cout << add(3, 4);
}