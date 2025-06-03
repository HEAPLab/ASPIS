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

// A function that allocates memory, uses it, and then deletes it
int main() {
    // Allocate a single integer on the heap
    int *p = new int(5);
    *p = 10;  // modify the allocated value
    int result = *p;
    delete p;

    // Print the result (non-duplicated)
    std::cout << "Value: " << result << std::endl;
    return 0;
}
