#include "string_processing.h"

std::vector<std::string_view> SplitIntoWordsStringView(std::string_view text) {
    std::vector<std::string_view> words;
    while (!text.empty()) {
        auto space = text.find(' ');
        if (space == text.npos) words.push_back(text.substr(0, text.size()));
        else  words.push_back(text.substr(0, space));
        text.remove_prefix(std::min(text.find_first_not_of(' ', space), text.size()));
    }
    return words;
}
