//
// Created by martina on 07/01/26.
//
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    srand(time(NULL));
    int y = rand() % 2;
    int r = 0;
    int a = y || r;
    printf("SUCCESS\n");
    return 0;
}
