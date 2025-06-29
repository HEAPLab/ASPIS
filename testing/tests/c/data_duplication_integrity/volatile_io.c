/* 
* This test verifies that a volatile variable (e.g., simulating memory-mapped I/O) 
* is not duplicated and remains correctly accessed without triggering ASPIS protections. 
*/

#include <stdio.h>

volatile int io_port = 42;  // Simulate memory-mapped I/O

int main() {
    int x = io_port; // Load from volatile
    printf("%d", x);
    return 0;
}

// expected output
// 42
