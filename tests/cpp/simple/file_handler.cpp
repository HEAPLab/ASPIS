#include <iostream>
#include <cstdio>
#include <string>

class FileHandler
{
public:
    FileHandler(const std::string &path)
    {
        file = fopen(path.c_str(), "w");
        if (file)
        {
            std::fputs("Hello, RAII!\n", file);
        }
    }

    ~FileHandler()
    {
        if (file)
        {
            std::fclose(file);
            std::cout << "File closed\n";
        }
    }

private:
    FILE *file;
};

int main()
{
    FileHandler handler("test_raii.txt");
    std::cout << "Handler created\n";
    return 0;
}
