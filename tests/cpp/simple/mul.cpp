#include <iostream>
#include <cstdlib>

void DataCorruption_Handler() {
    std::cerr << "Errore ASPIS: Data corruption rilevata\n";
    std::exit(1);
}
void SigMismatch_Handler() {
    std::cerr << "Errore ASPIS: Signature mismatch rilevata\n";
    std::exit(1);
}

int multiply(int a, int b)
{
    return a * b;
}

int main()
{
    int result = multiply(6, 7);
    std::cout << result << std::endl;
    return 0;
}
