#include <iostream>
#include <memory>
#include <vector>

class MyObject
{
public:
    MyObject(int v) : value(v) {}
    int get() const { return value; }

private:
    int value;
};

int main()
{
    std::vector<std::shared_ptr<MyObject>> vec;

    // Create 100 shared_ptr instances
    for (int i = 0; i < 100; ++i)
    {
        vec.push_back(std::make_shared<MyObject>(i));
    }

    // Transfer one shared_ptr into a unique_ptr to simulate move semantics
    std::unique_ptr<MyObject> unique = std::make_unique<MyObject>(42);

    long long sum = 0;
    for (const auto &ptr : vec)
    {
        sum += ptr->get();
    }

    sum += unique->get(); // Add unique_ptr value

    std::cout << sum << "\n"; // Expect 4950 + 42 = 4992
    return 0;
}
