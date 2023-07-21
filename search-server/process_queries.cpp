#include "process_queries.h"
#include <execution>
#include <utility>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries){
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par,
                   queries.begin(), queries.end(), result.begin(),
                   [&search_server](const std::string& query){
                       return search_server.FindTopDocuments(query);});
    return result;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries){
    std::list<Document> result;
    for (auto &docs : ProcessQueries(search_server, queries)){
        for (auto &doc : docs){
            result.push_back(std::move(doc));
        }
    }
    return result;
}


