#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX 1024

__attribute__((annotate("to_harden")))
int r = 0;

int main() {
    srand(time(NULL));
    r = rand() % MAX + 200;
    if ( r > 200 ) printf("r > 200\n");
    else if ( r > 100 ) printf("100 < r < 200\n");
    else if ( r > 50 ) printf("50 < r < 100\n");
    else printf("0 < r < 50  \n");

    return 0;
}
