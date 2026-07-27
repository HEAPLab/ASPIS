// target.c
#include <stdio.h>
#include <unistd.h>

int main() {
    int counter = 0;
    
    for (int i = 0; i < 10; i++) {
        counter += 2;
        printf("Iteration %d: counter = %d\n", i, counter);
    }
    
    printf("Final result: %d\n", counter);
    return 0;
}