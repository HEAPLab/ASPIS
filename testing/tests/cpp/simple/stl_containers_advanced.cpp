#include <iostream>
#include <vector>
#include <cstdlib>

// ASPIS error handling functions
void DataCorruption_Handler() {
    std::cerr << "Errore ASPIS: Data corruption rilevata\n";
    std::exit(EXIT_FAILURE);
}

void SigMismatch_Handler() {
    std::cerr << "Errore ASPIS: Signature mismatch rilevata\n";
    std::exit(EXIT_FAILURE);
}

int main() {
    // Example of advanced container usage with dynamic memory
    std::vector<int*> ptrVector;
    ptrVector.reserve(5);  // reserve capacity for efficiency

    // Allocate integers on the heap and store pointers in the vector
    for (int i = 0; i < 5; ++i) {
        ptrVector.push_back(new int(i));
    }

    // Output the elements pointed by the pointers in the vector
    for (int* p : ptrVector) {
        std::cout << *p << " ";
    }
    std::cout << std::endl;

    // Note: We intentionally avoid deleting the allocated integers here.
    // Under ASPIS instrumentation, deleting them (either manually or via RAII smart pointers)
    // could cause a double free (each allocation is tracked/duplicated).
    // The OS will reclaim this memory when the program exits.
    return 0;
}
