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
    std::cout << "Result: " << value << std::endl;
}

// Example of a function template
template <typename T>
T myMax(T a, T b) {
    return (a > b) ? a : b;
}

// Example of a simple accumulator class template
template <typename T>
class Accumulator {
public:
    Accumulator() : sum(T()) {}
    void add(T value) {
        sum += value;  // duplicated safely
    }
    T total() const {
        return sum;
    }
private:
    T sum;
};

int main() {
    int x = 42, y = 17;
    int maxVal = myMax<int>(x, y);  // 42

    Accumulator<int> acc;
    acc.add(5);
    acc.add(10);
    int sumVal = acc.total();  // 15

    printResult(maxVal);
    printResult(sumVal);
    return 0;
}
