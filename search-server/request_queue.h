#pragma once
#ifndef REQUESTQUEUE_H
#define REQUESTQUEUE_H
#include "search_server.h"
#include <deque>
#include <vector>

using namespace std;
class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    void CheckRequest(const vector<Document>& results);
    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate);
    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status);
    vector<Document> AddFindRequest(const string& raw_query);
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        bool result_is_zero = false;
    };
    deque<QueryResult> requests_;
    int zero_result_ = 0;
    const static int min_in_day_ = 1440;
    const SearchServer& server_;
};


#endif // REQUESTQUEUE_H
