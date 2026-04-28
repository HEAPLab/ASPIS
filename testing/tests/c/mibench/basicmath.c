/*
 basicmath_mibench.c — single-file version for ASPIS compilation
 Sources merged: cubic.c, isqrt.c, rad2deg.c, snipmath.h, sniptype.h, pi.h
*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned long  DWORD;


// Numeric constant: avoids calling atan() function that ASPIS cannot duplicate.
#ifndef PI
#define PI 3.14159265358979323846
#endif

void DataCorruption_Handler(void) { printf("DATA_CORRUPTION_DETECTED"); exit(0); }
void SigMismatch_Handler(void)    { printf("SIG_MISMATCH_DETECTED");    exit(0); }

__attribute__((annotate("to_duplicate")))
static double sqrt_w(double x)              { return sqrt(x); }
__attribute__((annotate("to_duplicate")))
static double cos_w(double x)               { return cos(x); }
__attribute__((annotate("to_duplicate")))
static double acos_w(double x)              { return acos(x); }
__attribute__((annotate("to_duplicate")))
static double pow_w(double x, double y)     { return pow(x, y); }
__attribute__((annotate("to_duplicate")))
static double fabs_w(double x)              { return fabs(x); }

__attribute__((annotate("to_duplicate")))
static void memcpy_to_duplicate(void *dst, const void *src, size_t n) {
    memcpy(dst, src, n);
}

struct int_sqrt {
    unsigned sqrt;
    unsigned frac;
};


// long double removed from original code since ASPIS can have issues 
void SolveCubic(double a, double b, double c, double d,
                int *solutions, double *x)
{
    double a1 = b / a, a2 = c / a, a3 = d / a;
    double Q  = (a1*a1 - 3.0*a2) / 9.0;
    double R  = (2.0*a1*a1*a1 - 9.0*a1*a2 + 27.0*a3) / 54.0;
    double R2_Q3 = R*R - Q*Q*Q;
    double theta;

    if (R2_Q3 <= 0) {
        *solutions = 3;
        theta = acos_w(R / sqrt_w(Q*Q*Q));
        x[0] = -2.0*sqrt_w(Q)*cos_w(theta / 3.0)            - a1/3.0;
        x[1] = -2.0*sqrt_w(Q)*cos_w((theta + 2.0*PI) / 3.0) - a1/3.0;
        x[2] = -2.0*sqrt_w(Q)*cos_w((theta + 4.0*PI) / 3.0) - a1/3.0;
    } else {
        *solutions = 1;
        x[0] = pow_w(sqrt_w(R2_Q3) + fabs_w(R), 1.0/3.0);
        x[0] += Q / x[0];
        x[0] *= (R < 0.0) ? 1 : -1;
        x[0] -= a1 / 3.0;
    }
}

// uint32_t enforces 32-bit behaviour; direct struct assignment avoids memcpy issues with ASPIS.
#define BITSPERLONG 32
#define TOP2BITS(x) (((x) & (3UL << (BITSPERLONG-2))) >> (BITSPERLONG-2))

void usqrt(unsigned long x, struct int_sqrt *q)
{
    uint32_t a = 0;
    uint32_t r = 0;
    uint32_t e = 0;
    int i;

    for (i = 0; i < BITSPERLONG; i++) {
        r = (r << 2) + TOP2BITS(x); x <<= 2;
        a <<= 1;
        e = (a << 1) + 1;
        if (r >= e) {
            r -= e;
            a++;
        }
    }
    q->sqrt = a;
    q->frac = 0;
}

double deg2rad(double deg)
{
    return (PI * deg / 180.0);
}

double rad2deg(double rad)
{
    return (180.0 * rad / PI);
}

int main(void)
{
    double x[3];
    int solutions;
    struct int_sqrt q;

    /*
     ── SolveCubic: (x-2)(x-3)(x-5) = x³ - 10x² + 31x - 30
     Expected: 3 real solutions {2, 3, 5}.
     Checked via Vieta's formulas (order-independent):
       x0+x1+x2        = -b/a = 10
       x0x1+x0x2+x1x2  =  c/a = 31
       x0*x1*x2         = -d/a = 30
    */
    SolveCubic(1.0, -10.0, 31.0, -30.0, &solutions, x);
    int cubic_ok = (solutions == 3)
                && (fabs_w(x[0]+x[1]+x[2]                      - 10.0) < 1e-6)
                && (fabs_w(x[0]*x[1]+x[0]*x[2]+x[1]*x[2]       - 31.0) < 1e-6)
                && (fabs_w(x[0]*x[1]*x[2]                       - 30.0) < 1e-6);

    /*
     ── usqrt(9): perfect square, portable fixed-point result
     sqrt(9) = 3 exactly → fixed-point (×2^16) = 3×65536 = 196608.
     For x=9 the result fits in 32 bits: same on 32-bit and 64-bit.
    */
    usqrt(9, &q);
    int usqrt_ok = (q.sqrt == 196608);

    /*
     ── deg2rad / rad2deg round-trip
     deg2rad(180°) = π, rad2deg(π) = 180°: the two constants cancel.
    */
    int angle_ok = (fabs_w(rad2deg(deg2rad(180.0)) - 180.0) < 1e-9);

    if (cubic_ok && usqrt_ok && angle_ok)
        printf("SUCCESS");
    else
        printf("FAIL");

    return 0;
}