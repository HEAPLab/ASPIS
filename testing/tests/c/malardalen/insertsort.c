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

 /* MDH WCET BENCHMARK SUITE. File version $Id: insertsort.c 7800 2018-01-19 12:43:12Z lkg02 $ */

/*************************************************************************/
/*                                                                       */
/*   SNU-RT Benchmark Suite for Worst Case Timing Analysis               */
/*   =====================================================               */
/*                              Collected and Modified by S.-S. Lim      */
/*                                           sslim@archi.snu.ac.kr       */
/*                                         Real-Time Research Group      */
/*                                        Seoul National University      */
/*                                                                       */
/*                                                                       */
/*        < Features > - restrictions for our experimental environment   */
/*                                                                       */
/*          1. Completely structured.                                    */
/*               - There are no unconditional jumps.                     */
/*               - There are no exit from loop bodies.                   */
/*                 (There are no 'break' or 'return' in loop bodies)     */
/*          2. No 'switch' statements.                                   */
/*          3. No 'do..while' statements.                                */
/*          4. Expressions are restricted.                               */
/*               - There are no multiple expressions joined by 'or',     */
/*                'and' operations.                                      */
/*          5. No library calls.                                         */
/*               - All the functions needed are implemented in the       */
/*                 source file.                                          */
/*                                                                       */
/*                                                                       */
/*************************************************************************/
/*                                                                       */
/*  FILE: insertsort.c                                                   */
/*  SOURCE : Public Domain Code                                          */
/*                                                                       */
/*  DESCRIPTION :                                                        */
/*                                                                       */
/*     Insertion sort for 10 integer numbers.                            */
/*     The integer array a[] is initialized in main function.            */
/*                                                                       */
/*  REMARK :                                                             */
/*                                                                       */
/*  EXECUTION TIME :                                                     */
/*                                                                       */
/*                                                                       */
/*************************************************************************/



#ifdef DEBUG
int cnt1, cnt2;
#endif

unsigned int a[11];

int main()
{
  int  i,j, temp;

  a[0] = 0;   /* assume all data is positive */
  a[1] = 11; a[2]=10;a[3]=9; a[4]=8; a[5]=7; a[6]=6; a[7]=5;
  a[8] =4; a[9]=3; a[10]=2;
  i = 2;
  while(i <= 10){
#ifdef DEBUG
      cnt1++;
#endif
      j = i;
#ifdef DEBUG
	cnt2=0;
#endif
      while (a[j] < a[j-1])
      {
#ifdef DEBUG
	cnt2++;
#endif
	temp = a[j];
	a[j] = a[j-1];
	a[j-1] = temp;
	j--;
      }
#ifdef DEBUG
	printf("Inner Loop Counts: %d\n", cnt2);
#endif
      i++;
    }
#ifdef DEBUG
    printf("Outer Loop : %d ,  Inner Loop : %d\n", cnt1, cnt2);
#endif
//added code for checking the sorted array (expected_output)
    printf("Sorted array: ");
    for(int k = 0; k <= 10; k++) {
        printf("%d ", a[k]);
    }
    printf("\n");
//end of added code
    return 0;
}