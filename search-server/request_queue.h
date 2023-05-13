#pragma once
#include "search_server.h"
#include <deque>
#include <vector>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    void CheckRequest(const std::vector<Document>& results);
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        bool result_is_zero = false;
    };
    std::deque<QueryResult> requests_;
    int zero_result_ = 0;
    const static int min_in_day_ = 1440;
    const SearchServer& server_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    const auto search_results = server_.FindTopDocuments(raw_query, document_predicate);
    CheckRequest(search_results);
    return search_results;
}


