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

// Print function (non-duplicated)
__attribute__((no_duplicate))
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
