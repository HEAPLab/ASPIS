#include <stdio.h>
#include <stdlib.h>

/* -------------------------------------------------------------------
 * ASPIS fault handlers - required by the hardening pipeline.
 * DataCorruption_Handler: called when a duplicated variable mismatch
 *                         is detected (EDDI/sEDDI/FDSC).
 * SigMismatch_Handler:    called when a control-flow violation is
 *                         detected (CFCSS/RASM).
 * Customise these to log, signal, or exit as needed for your
 * fault injection experiments.
 * ------------------------------------------------------------------- */
void DataCorruption_Handler(void) {
    fprintf(stderr, "FAULT_DETECTED: DataCorruption\n");
    exit(2);
}

void SigMismatch_Handler(void) {
    fprintf(stderr, "FAULT_DETECTED: SigMismatch\n");
    exit(3);
}

/* -------------------------------------------------------------------
 * Application entry point.
 * Replace the body with your program logic.
 * ------------------------------------------------------------------- */

 
/*
    * This is a simple program that demonstrates the use of multiple functions.
    * The main function initializes an array and calls the expo function to
    * compute the square of the input values. The results are stored in an array
    * and printed to the console.
    *
    * The expo function takes an integer input, squares it, and returns the result.
    *
    * This program is structured to allow for easy testing and fault injection,
    * with handlers for data corruption and signal mismatches.
    * The program uses a simple loop to fill an array with the squares of two different values,
    * demonstrating the use of conditional logic and function calls. 
*/


int expo(int);

int main(int argc, char **argv) {
    int i = 0;
    int j = 2;
    int w = 3;
    int z[10];
    int flag = 1;

    while(flag){
        if(i % 2 == 0){
            z[i] = expo(j);
        }else{
            z[i] = expo(w);
        }
        i++;
        if(i == 10){
            flag = 0;
        }
    }

    for(i = 0; i < 10; i++){
        printf("z[%d] = %d\n", i, z[i]);
    }

    return 0;
}

int expo(int x){

    x = x * x;

    return x;
}
