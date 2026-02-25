#include <stdio.h>

int foo() {
    return 0;
}

int main() {
    int sasso_carta = 1;
    int filippo_congenito = 2;
    for(int i = foo(); i < sasso_carta + filippo_congenito; i++)
	filippo_congenito--;
    printf("%d", filippo_congenito);
    return 0;
}
