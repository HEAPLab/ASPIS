#include <iostream>
#include <thread>
#include <atomic>
#include <cstdlib>


extern "C"
__attribute__((annotate("runtime_sig")))
thread_local int runtime_sig = -0xDEAD;

extern "C"
__attribute__((annotate("run_adj_sig")))
thread_local int run_adj_sig = -0xDEAD;

// ASPIS error handlers
extern "C" __attribute__((no_duplicate))
void DataCorruption_Handler() {
    std::cerr << "ASPIS error: Data corruption detected\n";
    std::exit(EXIT_FAILURE);
}

extern "C" __attribute__((no_duplicate))
void SigMismatch_Handler() {
    std::cerr << "ASPIS error: Signature mismatch detected\n";
    std::exit(EXIT_FAILURE);
}

std::atomic<int> counter{0};

// Worker function marked as non-duplicable due to atomic update
__attribute__((no_duplicate))
void worker(int times) {
    int local_count = 0;
    for (int i = 0; i < times; ++i) {
        local_count++;
    }
    counter.fetch_add(local_count, std::memory_order_relaxed);
}

// Thread launching logic is marked non-duplicable
__attribute__((no_duplicate))
void run_all_threads(int num_threads, int work_per_thread) {
    for (int i = 0; i < num_threads; ++i) {
        std::thread t(worker, work_per_thread);
        t.join();
    }
}

int main() {
    const int num_threads = 4;
    const int work_per_thread = 1000;

    run_all_threads(num_threads, work_per_thread);

    int expected = num_threads * work_per_thread;
    int observed = counter.load();
    std::cout << "Final counter value: " << observed << std::endl;
    return observed == expected ? 0 : 1;
}
