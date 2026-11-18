/*
 basicmath_mibench.c — single-file version for ASPIS compilation
 Sources merged: cubic.c, isqrt.c, rad2deg.c, snipmath.h, sniptype.h, pi.h
*/

// Keep the names of the original C code
#![allow(
    non_snake_case,
    non_camel_case_types,
    non_upper_case_globals,
    dead_code
)]

use aspis_rust_annotations::aspis;

type BYTE = u8;
type WORD = u16;
type DWORD = u64;

// Numeric constant: avoids calling atan() function that ASPIS cannot duplicate.
const PI: f64 = 3.14159265358979323846;

#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    print!("DATA_CORRUPTION_DETECTED");
    std::process::exit(0);
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    print!("SIG_MISMATCH_DETECTED");
    std::process::exit(0);
}

#[aspis(to_duplicate)]
fn sqrt_w(x: f64) -> f64 {
    return x.sqrt();
}

#[aspis(to_duplicate)]
fn cos_w(x: f64) -> f64 {
    return x.cos();
}

#[aspis(to_duplicate)]
fn acos_w(x: f64) -> f64 {
    return x.acos();
}

#[aspis(to_duplicate)]
fn pow_w(x: f64, y: f64) -> f64 {
    return x.powf(y);
}

#[aspis(to_duplicate)]
fn fabs_w(x: f64) -> f64 {
    return x.abs();
}

#[aspis(to_duplicate)]
unsafe fn memcpy_to_duplicate(dst: *mut u8, src: *const u8, n: usize) {
    unsafe {
        std::ptr::copy_nonoverlapping(src, dst, n);
    }
}

struct int_sqrt {
    sqrt: u32,
    frac: u32,
}

// long double removed from original code since ASPIS can have issues
fn SolveCubic(a: f64, b: f64, c: f64, d: f64, solutions: &mut i32, x: &mut [f64; 3]) {
    let a1 = b / a;
    let a2 = c / a;
    let a3 = d / a;
    let Q = (a1 * a1 - 3.0 * a2) / 9.0;
    let R = (2.0 * a1 * a1 * a1 - 9.0 * a1 * a2 + 27.0 * a3) / 54.0;
    let R2_Q3 = R * R - Q * Q * Q;
    let theta: f64;

    if R2_Q3 <= 0.0 {
        *solutions = 3;
        theta = acos_w(R / sqrt_w(Q * Q * Q));
        x[0] = -2.0 * sqrt_w(Q) * cos_w(theta / 3.0) - a1 / 3.0;
        x[1] = -2.0 * sqrt_w(Q) * cos_w((theta + 2.0 * PI) / 3.0) - a1 / 3.0;
        x[2] = -2.0 * sqrt_w(Q) * cos_w((theta + 4.0 * PI) / 3.0) - a1 / 3.0;
    } else {
        *solutions = 1;
        x[0] = pow_w(sqrt_w(R2_Q3) + fabs_w(R), 1.0 / 3.0);
        x[0] += Q / x[0];
        x[0] *= if R < 0.0 { 1.0 } else { -1.0 };
        x[0] -= a1 / 3.0;
    }
}

// u32 enforces 32-bit behaviour; direct struct assignment avoids memcpy issues with ASPIS.
const BITSPERLONG: u32 = 32;
macro_rules! TOP2BITS {
    ($x:expr) => {
        (($x) & (3u64 << (BITSPERLONG - 2))) >> (BITSPERLONG - 2)
    };
}

fn usqrt(mut x: u64, q: &mut int_sqrt) {
    let mut a: u32 = 0;
    let mut r: u32 = 0;
    let mut e: u32;
    let mut i: u32;

    i = 0;
    while i < BITSPERLONG {
        r = (r << 2) + TOP2BITS!(x) as u32;
        x <<= 2;
        a <<= 1;
        e = (a << 1) + 1;
        if r >= e {
            r -= e;
            a += 1;
        }
        i += 1;
    }
    q.sqrt = a;
    q.frac = 0;
}

fn deg2rad(deg: f64) -> f64 {
    return PI * deg / 180.0;
}

fn rad2deg(rad: f64) -> f64 {
    return 180.0 * rad / PI;
}

fn main() {
    let mut x: [f64; 3] = [0.0; 3];
    let mut solutions: i32 = 0;
    let mut q = int_sqrt { sqrt: 0, frac: 0 };

    /*
     ── SolveCubic: (x-2)(x-3)(x-5) = x³ - 10x² + 31x - 30
     Expected: 3 real solutions {2, 3, 5}.
     Checked via Vieta's formulas (order-independent):
       x0+x1+x2        = -b/a = 10
       x0x1+x0x2+x1x2  =  c/a = 31
       x0*x1*x2         = -d/a = 30
    */
    SolveCubic(1.0, -10.0, 31.0, -30.0, &mut solutions, &mut x);
    let cubic_ok = (solutions == 3)
        && (fabs_w(x[0] + x[1] + x[2] - 10.0) < 1e-6)
        && (fabs_w(x[0] * x[1] + x[0] * x[2] + x[1] * x[2] - 31.0) < 1e-6)
        && (fabs_w(x[0] * x[1] * x[2] - 30.0) < 1e-6);

    /*
     ── usqrt(9): perfect square, portable fixed-point result
     sqrt(9) = 3 exactly → fixed-point (×2^16) = 3×65536 = 196608.
     For x=9 the result fits in 32 bits: same on 32-bit and 64-bit.
    */
    usqrt(9, &mut q);
    let usqrt_ok = q.sqrt == 196608;

    /*
     ── deg2rad / rad2deg round-trip
     deg2rad(180°) = π, rad2deg(π) = 180°: the two constants cancel.
    */
    let angle_ok = fabs_w(rad2deg(deg2rad(180.0)) - 180.0) < 1e-9;

    if cubic_ok && usqrt_ok && angle_ok {
        print!("SUCCESS");
    } else {
        print!("FAIL");
    }
}
