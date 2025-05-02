#include <iostream>

int main()
{
    const int N = 100000;
    int **arr = new int *[N];

    for (int i = 0; i < N; ++i)
    {
        arr[i] = new int(i);
    }

    long long sum = 0;
    for (int i = 0; i < N; ++i)
    {
        sum += *arr[i];
        delete arr[i];
    }

    delete[] arr;
    std::cout << sum << "\n";
    return 0;
}
