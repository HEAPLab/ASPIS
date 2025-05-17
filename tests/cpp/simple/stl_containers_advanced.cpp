#include <iostream>
#include <map>
#include <set>
#include <string>

void test_stl_containers()
{
    std::map<std::string, int> word_count;
    std::set<std::string> unique_words;

    std::string words[] = {"apple", "banana", "apple", "orange", "banana", "grape"};

    for (const auto &word : words)
    {
        ++word_count[word];
        unique_words.insert(word);
    }

    for (const auto &[word, count] : word_count)
    {
        std::cout << word << ": " << count << "\n";
    }

    for (const auto &word : unique_words)
    {
        std::cout << word << "\n";
    }
}

int main()
{
    test_stl_containers();
    return 0;
}
