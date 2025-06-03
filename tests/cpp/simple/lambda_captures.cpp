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

// Executes a function/lambda in a non-duplicated context
template <typename F>
__attribute__((no_duplicate))
void runNoDup(F func) {
    func();
}

int main() {
    // Example 1: lambda capturing a local variable by reference
    int x = 0;
    auto incrX = [&x](int val) {
        x += val;
        std::cout << "x incremented by " << val << std::endl;
    };
    incrX(5);  // can be duplicated safely (x is local to each duplicate)

    // Example 2: lambda capturing a heap-allocated pointer
    int *p = new int(10);
    auto incPtr = [p]() {
        (*p)++;
    };
    runNoDup(incPtr);  // non-duplicated increment of shared memory

    std::cout << "Value pointed by p: " << *p << std::endl;
    delete p;
    return 0;
}
