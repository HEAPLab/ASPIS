#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

std::atomic<int> counter{0};

void worker(int times)
{
    for (int i = 0; i < times; ++i)
    {
        counter.fetch_add(1);
    }
}

int main()
{
    const int num_threads = 4;
    const int work_per_thread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(worker, work_per_thread);
    }

    for (auto &t : threads)
    {
        t.join();
    }

    std::cout << counter << "\n";
    return 0;
}
