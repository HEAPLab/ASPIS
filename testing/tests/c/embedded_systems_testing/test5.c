#include <stdio.h>
#include <string.h>

int main() {
    
    int var = 0;

    switch (var)
    {
    case 0:
        printf("0\n");
        break;
    case 1:
        printf("1\n");
        break;
    case 2:
        printf("2\n");
        break;
    default:
        printf("default\n");
        break;
    }

    return 0;
}