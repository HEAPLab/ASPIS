/*
 * Miscellaneous operations on annotated globals.
 */

#include <stdio.h>

void DataCorruption_Handler(void) {}
void SigMismatch_Handler(void) {}

__attribute__((annotate("to_duplicate")))
int duplicated_global = 100;

__attribute__((annotate("exclude")))
int excluded_global = 200;

int increment(int x) {
    return x + 1;
}

__attribute__((annotate("to_duplicate")))
int multiply_by_two(int x) {
    return x * 2;
}

__attribute__((annotate("exclude")))
int secret_func(int x) {
    return x - 42;
}

int main() {
    int val = duplicated_global;  
    val = increment(val);         
    val = multiply_by_two(val);  

    int excl = excluded_global;
    excl += 5;

    int secret = secret_func(excl);

    int result = val + excl + secret;

    if (result == 570) {
        printf("OK");
    } else {
        printf("FAIL");
    }
    return 0;
}

// expected output
// OK
