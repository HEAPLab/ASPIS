/*
This test verifies that values of vector type (e.g., <2 x i64>) are compared correctly
at the synchronization points, instead of being indexed like arrays. HashMap's
generic code, compiled into this crate, uses loads and stores of vector type internally.
*/

use aspis_rust_annotations::aspis;
use std::collections::HashMap;

// ASPIS error handlers (non-duplicated)
#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    eprintln!("ASPIS error: data corruption detected");
    std::process::exit(1);
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    eprintln!("ASPIS error: signature mismatch detected");
    std::process::exit(1);
}

#[aspis(to_harden)]
fn count_words(text: &str) -> HashMap<&str, u64> {
    let mut counts = HashMap::new();
    for word in text.split_whitespace() {
        *counts.entry(word).or_insert(0) += 1;
    }
    counts
}

fn main() {
    let counts = count_words("the quick brown fox jumps over the lazy dog the end");
    // Print in a fixed order: the HashMap iteration order is random
    for word in ["the", "fox", "dog", "cat"] {
        println!("{}: {}", word, counts.get(word).copied().unwrap_or(0));
    }
}

// expected output
// the: 3
// fox: 1
// dog: 1
// cat: 0
