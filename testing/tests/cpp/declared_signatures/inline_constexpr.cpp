#include <iostream>

int runtime_sig;

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

void printResult(int value) {
    std::cout << value << std::endl;
}

// Inline function example (calculates square)
inline int square(int x) {
    return x * x;
}

// constexpr function example
constexpr int getFive() {
    return 5;
}

int main() {
    int a = 3;
    int b = 4;

    int result1 = square(a);  // 9
    int result2 = square(b);  // 16
    constexpr int c = getFive();  // 5

    printResult(result1);
    printResult(result2);
    printResult(c);

    return 0;
}
