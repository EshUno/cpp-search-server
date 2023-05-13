#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : server_(search_server) {

}

void RequestQueue::CheckRequest(const vector<Document>& results){
    if (results.empty()){
        ++ zero_result_;
        requests_.push_back({true});
    }
    else requests_.push_back({false});

    if (requests_.size() >= min_in_day_ + 1){
        if (requests_.front().result_is_zero == true) --zero_result_;
        requests_.pop_front();
    }
}

template <typename DocumentPredicate>
vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
    const auto search_results = server_.FindTopDocuments(raw_query, document_predicate);
    CheckRequest(search_results);
    return search_results;
}
vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    const auto search_results = server_.FindTopDocuments(raw_query, status);
    CheckRequest(search_results);
    return search_results;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    const auto search_results = server_.FindTopDocuments(raw_query);
    CheckRequest(search_results);
    return search_results;
}

int RequestQueue::GetNoResultRequests() const {
    return zero_result_;
}

