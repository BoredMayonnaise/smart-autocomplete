#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Offline asset preparation only; runtime dictionary validation remains in smart_core.
bool supported(std::string word) {
    for (char& c : word) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto letter=[](char c){return c>='a' && c<='z';};
    if(word.empty() || word.size()>32 || !letter(word.front()) || !letter(word.back())) return false;
    for(std::size_t i=0;i<word.size();++i)
        if(!letter(word[i]) && !((word[i]=='\'' || word[i]=='-') && i>0 && i+1<word.size() && letter(word[i-1]) && letter(word[i+1]))) return false;
    return true;
}
std::set<std::string> read_words(const char* path) {
    std::ifstream input(path);
    if(!input) throw std::runtime_error(std::string("Cannot read ")+path);
    std::set<std::string> result;
    for(std::string word;std::getline(input,word);) {
        std::string normalized;
        for (char c : word) if (c != '\r') normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        if (supported(normalized)) result.insert(normalized);
    }
    return result;
}
int main(int argc,char** argv) {
    try {
        if(argc!=6) throw std::runtime_error("Usage: make_dictionary teaching-words.txt english-words.10 english-words.20 english-words.35 system-words.txt");
        auto selected=read_words(argv[1]);
        auto common=read_words(argv[2]);selected.insert(common.begin(),common.end());
        if(selected.size()>5000) throw std::runtime_error("Level 10 plus teaching vocabulary exceeds limit");
        auto make_pool=[&](int start_index) {
            std::set<std::string> additional;
            for (int i=start_index;i<argc;++i) {
                auto words=read_words(argv[i]);
                for (const auto& word:words) if (!selected.count(word)) additional.insert(word);
            }
            return additional;
        };
        auto fill=[&](std::size_t target, std::set<std::string>& pool) {
            const auto remaining=target-selected.size();
            if(pool.size()<remaining) throw std::runtime_error("Not enough source words");
            std::vector<std::string> words(pool.begin(),pool.end());
            for(std::size_t i=0;i<remaining;++i) selected.insert(words[i*words.size()/remaining]);
        };
        // Reproduce the original 5,000-word bundle before expanding, preserving prior vocabulary.
        auto pool=make_pool(3);
        fill(5000,pool);
        pool=make_pool(3);
        fill(10000,pool);
        pool=make_pool(3);
        fill(50000,pool);
        if(selected.size()!=50000) throw std::runtime_error("Expected exactly 50,000 unique words");
        for(const auto& word:selected) std::cout<<word<<'\n';
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
    return 0;
}
