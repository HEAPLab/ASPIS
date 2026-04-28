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


/* MDH WCET BENCHMARK SUITE. File version $Id: matmult.c 7800 2018-01-19 12:43:12Z lkg02 $ */

/* matmult.c */
/* was mm.c! */


/*----------------------------------------------------------------------*
 * To make this program compile under our assumed embedded environment,
 * we had to make several changes:
 * - Declare all functions in ANSI style, not K&R.
 *   this includes adding return types in all cases!
 * - Declare function prototypes
 * - Disable all output
 * - Disable all UNIX-style includes
 *
 * This is a program that was developed from mm.c to matmult.c by
 * Thomas Lundqvist at Chalmers.
 *----------------------------------------------------------------------*/
#define UPPSALAWCET 1


/* ***UPPSALA WCET***:
   disable stupid UNIX includes */
#ifndef UPPSALAWCET
#include <sys/types.h>
#include <sys/times.h>
#endif

/*
 * MATRIX MULTIPLICATION BENCHMARK PROGRAM:
 * This program multiplies 2 square matrices resulting in a 3rd
 * matrix. It tests a compiler's speed in handling multidimensional
 * arrays and simple arithmetic.
 */

#define UPPERLIMIT 20

typedef int matrix [UPPERLIMIT][UPPERLIMIT];

int Seed;
matrix ArrayA, ArrayB, ResultArray;
//added code for checking the sorted array (expected_output)
static const matrix correct_result = {
   {291018000, 315000075, 279049970, 205074215, 382719905, 302595865, 348060915, 308986330, 343160760, 307099935, 292564810, 240954510, 232755815, 246511665, 328466830, 263664375, 324016395, 334656070, 285978755, 345370360},
   {252241835, 333432715, 299220275, 247745815, 422508990, 316728505, 359662270, 277775280, 323336795, 320656600, 249903690, 251499360, 242195700, 263484280, 348207635, 289485100, 328607555, 300799835, 269351410, 305703460},
   {304901010, 316252815, 263230275, 208939015, 421993740, 335002930, 348571170, 280992155, 289749970, 259701175, 295249990, 310900035, 250896625, 250154105, 315096035, 236364800, 312879355, 312580685, 275998435, 344137885},
   {286700525, 325985600, 253054970, 224361490, 353502130, 306544290, 323492140, 259123905, 307731610, 282414410, 281127810, 246936935, 207890815, 233789540, 339836730, 277296350, 319925620, 307470895, 290537580, 292297535},
   {272571255, 377663320, 304545985, 263001340, 375034885, 325423710, 410620380, 313191730, 356989815, 308508355, 218003850, 272487135, 266000220, 264734710, 367539620, 304146675, 355295500, 276019740, 251415695, 301225235},
   {272547900, 321522300, 288294345, 247748015, 389912855, 331874890, 370798315, 315467255, 367554485, 311947660, 258809685, 270536510, 256730515, 287143040, 363087030, 285672775, 353670120, 304219695, 274897255, 324684660},
   {233123995, 227142480, 212655155, 198592290, 345335250, 302661845, 253374925, 233243305, 233750030, 224590040, 200404820, 250791135, 234405760, 211723645, 280630165, 185245875, 296423665, 276278575, 252368265, 278726535},
   {277690535, 339615440, 320921550, 307114315, 400187215, 334374655, 376286920, 295993530, 362988020, 356272700, 293965465, 261574710, 259690975, 263037705, 416748985, 274683275, 385571030, 402782385, 323927010, 362778710},
   {267168970, 323401680, 279474330, 201934365, 362624300, 330736145, 371793675, 299650280, 333646005, 264791490, 215918320, 277512760, 264068435, 234555295, 321772515, 217507025, 310372440, 317544750, 245525965, 343183435},
   {281293570, 326519505, 233494705, 238516065, 297038200, 266273420, 349521550, 259343530, 306032255, 266397915, 210274920, 263743085, 231689610, 251949545, 293562740, 226822900, 309225440, 286212000, 206108715, 236678985},
   {288404350, 310319375, 282695670, 244150740, 426489380, 387525790, 342018190, 326086505, 352250260, 319997735, 300645835, 284822660, 271837440, 274000415, 361826730, 252399600, 348582320, 375813820, 316588255, 322499110},
   {273368780, 329706295, 288668335, 234501665, 381962610, 343186285, 337520205, 259637405, 295755465, 284778105, 205310525, 249598310, 256662470, 251533535, 336159770, 249342150, 333559450, 329296590, 278254845, 300673860},
   {318589575, 315522800, 260632295, 250009765, 337127730, 312810490, 346698590, 260810030, 388289910, 337081285, 283635410, 208148610, 234123865, 259653165, 370115255, 243311450, 377808245, 358786770, 286839730, 321912835},
   {229541925, 253967450, 223002545, 202302515, 303446955, 268472740, 285580065, 211013405, 287677960, 279773910, 227377310, 197461135, 222469715, 179536615, 306957380, 178407075, 281051570, 279718120, 234868230, 288991535},
   {290692955, 317729070, 297868235, 213450065, 469270935, 375344910, 326987580, 334565680, 325300040, 290325655, 254703825, 284914960, 245773820, 276641510, 323510795, 271034400, 337424250, 360011440, 281515520, 331261535},
   {287075125, 313194850, 269889345, 208109115, 420653930, 331900290, 355440665, 318065155, 343785360, 302163035, 308959360, 312666110, 268997740, 288557415, 370158305, 205012650, 318198795, 384484520, 316450105, 378714460},
   {278680580, 356815220, 307597060, 216073365, 390879235, 358775185, 358895230, 306434180, 315569040, 272688130, 249424325, 274584610, 273530970, 265450585, 325127920, 312802050, 317134900, 298518590, 269975470, 332586535},
   {245629780, 267021570, 234689035, 208808065, 366356035, 267059560, 349348005, 270158755, 348048340, 291550930, 272717800, 259714410, 236033845, 280627610, 335089770, 176610475, 259339950, 322752840, 236218295, 329687310},
   {226517370, 272306005, 271484080, 216145515, 400972075, 288475645, 332969550, 338410905, 329052205, 330392265, 306488095, 271979085, 232795960, 257593945, 339558165, 202700275, 320622065, 386350450, 315344865, 329233410},
   {224852610, 231292540, 236945875, 243273740, 336327040, 305144680, 248261920, 191671605, 241699245, 263085200, 198883715, 175742885, 202517850, 172427630, 296304160, 209188850, 326546955, 252990460, 238844535, 289753485}
};
//end of added code

#ifdef UPPSALAWCET
/* Our picky compiler wants prototypes! */
void Multiply(matrix A, matrix B, matrix Res);
void InitSeed(void);
void Test(matrix A, matrix B, matrix Res);
void Initialize(matrix Array);
int RandomInteger(void);
#endif

int main()
{
   InitSeed();
/* ***UPPSALA WCET***:
   no printing please! */
#ifndef UPPSALAWCET
   printf("\n   *** MATRIX MULTIPLICATION BENCHMARK TEST ***\n\n");
   printf("RESULTS OF THE TEST:\n");
#endif
   Test(ArrayA, ArrayB, ResultArray);
   return 0;
}


void InitSeed(void)
/*
 * Initializes the seed used in the random number generator.
 */
{
  /* ***UPPSALA WCET***:
     changed Thomas Ls code to something simpler.
   Seed = KNOWN_VALUE - 1; */
  Seed = 0;
}


void Test(matrix A, matrix B, matrix Res)
/*
 * Runs a multiplication test on an array.  Calculates and prints the
 * time it takes to multiply the matrices.
 */
{
#ifndef UPPSALAWCET
   long StartTime, StopTime;
   float TotalTime;
#endif

   Initialize(A);
   Initialize(B);

   /* ***UPPSALA WCET***: don't print or time */
#ifndef UPPSALAWCET
   StartTime = ttime ();
#endif

   Multiply(A, B, Res);
//added code for checking the sorted array (expected_output)
   int i, j;
   for (i = 0; i < UPPERLIMIT; i++) {
     for (j = 0; j < UPPERLIMIT; j++) {
       if (Res[i][j] != correct_result[i][j]) {
         printf("FAIL\n");
         return;
         }
      }
   }
   printf("SUCCESS\n");
//end of added code
   
   /* ***UPPSALA WCET***: don't print or time */
#ifndef UPPSALAWCET
   StopTime = ttime();
   TotalTime = (StopTime - StartTime) / 1000.0;
   printf("    - Size of array is %d\n", UPPERLIMIT);
   printf("    - Total multiplication time is %3.3f seconds\n\n", TotalTime);
#endif


}


void Initialize(matrix Array)
/*
 * Intializes the given array with random integers.
 */
{
   int OuterIndex, InnerIndex;

   for (OuterIndex = 0; OuterIndex < UPPERLIMIT; OuterIndex++)
      for (InnerIndex = 0; InnerIndex < UPPERLIMIT; InnerIndex++)
         Array[OuterIndex][InnerIndex] = RandomInteger();
}


int RandomInteger(void)
/*
 * Generates random integers between 0 and 8095
 */
{
   Seed = ((Seed * 133) + 81) % 8095;
   return (Seed);
}


#ifndef UPPSALAWCET
int ttime()
/*
 * This function returns in milliseconds the amount of compiler time
 * used prior to it being called.
 */
{
   struct tms buffer;
   int utime;

   /*   times(&buffer);   times not implemented */
   utime = (buffer.tms_utime / 60.0) * 1000.0;
   return (utime);
}
#endif

void Multiply(matrix A, matrix B, matrix Res)
/*
 * Multiplies arrays A and B and stores the result in ResultArray.
 */
{
   int Outer, Inner, Index;

   for (Outer = 0; Outer < UPPERLIMIT; Outer++)
      for (Inner = 0; Inner < UPPERLIMIT; Inner++)
      {
         Res [Outer][Inner] = 0;
         for (Index = 0; Index < UPPERLIMIT; Index++)
            Res[Outer][Inner]  +=
               A[Outer][Index] * B[Index][Inner];
       }
}
