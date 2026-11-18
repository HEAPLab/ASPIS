// Keep the names of the original C++ code
#![allow(non_snake_case, non_upper_case_globals, unused_variables)]

// ASPIS handlers
#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    eprintln!("Data corruption detected");
    std::process::exit(1);
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    eprintln!("Signature mismatch detected");
    std::process::exit(1);
}

// Rust structs cannot have static members: this is FakeFileHandler's one.
static mut resource_opened: bool = false;

// Simulate a resource (like a file) using a static variable.
// This avoids file-system side effects which are problematic under duplication.
struct FakeFileHandler {}

impl FakeFileHandler {
    // Simulate resource acquisition
    fn new() -> Self {
        let handler = FakeFileHandler {};
        handler.openResource();
        println!("Handler created");
        handler
    }

    // Prevent accidental copying: Rust structs are not copyable unless they
    // implement Clone or Copy.

    fn openResource(&self) {
        // Simulate opening (can increment a static int, etc.)
        unsafe { resource_opened = true };
    }
    fn closeResource(&self) {
        // Simulate closing
        unsafe { resource_opened = false };
    }
}

// Simulate resource release
impl Drop for FakeFileHandler {
    fn drop(&mut self) {
        self.closeResource();
        println!("File closed");
    }
}

fn main() {
    // The RAII pattern is tested here: creation and destruction
    let handler = FakeFileHandler::new();
    // Handler destroyed automatically at end of scope
}
