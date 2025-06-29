#include <iostream>
#include <cstring> 

// ASPIS error handlers (non-duplicated)
__attribute__((no_duplicate))
void DataCorruption_Handler() {
    std::cerr << "ASPIS error: Data corruption detected\n";
}

__attribute__((no_duplicate))
void SigMismatch_Handler() {
    std::cerr << "ASPIS error: Signature mismatch detected\n";
}

// Helper to print two pointer values (non-duplicated)
__attribute__((no_duplicate))
void printPointers(int *p1, int *p2) {
    std::cout << "Value pointed by p1: " << *p1;
    if (p2) {
        std::cout << ", Value pointed by p2: " << *p2;
    }
    std::cout << std::endl;
}

int main() {
    // Allocate a small array on the heap
    int *buffer = new int[2];
    buffer[0] = 100;
    buffer[1] = 200;
    // Use a single pointer + offset instead of two aliases
    int *base = buffer;
    int secondValue = *(base + 1);

    // Example struct with two distinct heap allocations
    struct Pair { int a; int b; };
    Pair *obj = new Pair{1, 2};

    // Example of copying data instead of aliasing
    int *p1 = new int(42);
    // int *p2 = p1;  // removed aliasing
    int *p2 = new int(*p1);  // copy the value

    // Print the results (non-duplicated)
    printPointers(p1, p2);

    delete[] buffer;
    delete obj;
    delete p1;
    delete p2;
    return 0;
}
