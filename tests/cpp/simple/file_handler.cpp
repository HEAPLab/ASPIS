#include <iostream>
#include <cstdlib>

// ASPIS handlers
extern "C" __attribute__((no_duplicate))
void DataCorruption_Handler() {
    std::cerr << "Data corruption detected" << std::endl;
    std::exit(EXIT_FAILURE);
}

extern "C" __attribute__((no_duplicate))
void SigMismatch_Handler() {
    std::cerr << "Signature mismatch detected" << std::endl;
    std::exit(EXIT_FAILURE);
}

// Simulate a resource (like a file) using a static variable.
// This avoids file-system side effects which are problematic under duplication.
class FakeFileHandler {
public:
    // Simulate resource acquisition
    FakeFileHandler() {
        openResource();
        std::cout << "Handler created" << std::endl;
    }

    // Simulate resource release (RAII)
    ~FakeFileHandler() {
        closeResource();
        std::cout << "File closed" << std::endl;
    }

    // Prevent accidental copying
    FakeFileHandler(const FakeFileHandler&) = delete;
    FakeFileHandler& operator=(const FakeFileHandler&) = delete;

private:
    void openResource() {
        // Simulate opening (can increment a static int, etc.)
        resource_opened = true;
    }
    void closeResource() {
        // Simulate closing
        resource_opened = false;
    }
    inline static bool resource_opened = false;
};

int main() {
    // The RAII pattern is tested here: creation and destruction
    FakeFileHandler handler;
    // Handler destroyed automatically at end of scope
    return 0;
}
