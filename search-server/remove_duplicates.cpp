#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server){
    const auto MakeGluedString = [](const auto& freq) {
        std::string res;
        res.reserve(1024);
        for (auto x : freq){
            res.append(x.first);
        }
        return res;
    };

    std::unordered_set<std::string> content_hash;
    std::vector<int> ids_to_remove;

    for (auto i: search_server){
        auto freq = search_server.GetWordFrequencies(i);
        std::string content = MakeGluedString(freq);
        if (content_hash.count(content) > 0){
            ids_to_remove.push_back(i);
            std::cout << "Found duplicate document id " << i << std::endl;
        }
        else{
            content_hash.emplace(content);
        }
    }
    for (auto id : ids_to_remove)
    {
        search_server.RemoveDocument(id);
    }
}
