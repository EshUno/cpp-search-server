#pragma once
#include <future>
#include <string>
#include <vector>
#include <unordered_set>
#include <map>
#include <cmath>
#include <iterator>
#include <algorithm>
#include <execution>

#include "document.h"
#include "string_processing.h"
#include <string_view>
#include <deque>
#include "concurrent_map.h"
#include "log_duration.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        for (const auto& word : stop_words_){
            if (!IsValidWord(word)) throw std::invalid_argument("Недопустимый формат слов");
        }
    }

    explicit SearchServer(const std::string& stop_words_text): SearchServer(std::string_view(stop_words_text)) {}
    explicit SearchServer(const std::string_view stop_words_text): SearchServer(SplitIntoWordsStringView(stop_words_text)) {}

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    int GetDocumentCount() const;
    int GetDocumentId(int index) const;

    std::set<int>::const_iterator begin();
    std::set<int>::const_iterator end();

    using MatchedDocument = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    MatchedDocument MatchDocument(std::string_view raw_query, int document_id) const;
    MatchedDocument MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const;
    MatchedDocument MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::sequenced_policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy, int document_id);

private:
    //хранит все слова в виде строк (т.е. не удаляет их никогда)
    std::deque<std::string> storage_;
    const std::set<std::string, std::less<>> stop_words_;
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    mutable std::map<int, std::map<std::string_view, double>> word_freqs_;
    std::set<int> ids_;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    QueryWord ParseQueryWord(std::string_view text) const;

    Query ParseQuery(std::string_view text) const;
    template <typename ExecutionPolicy>
    Query ParseQuery(const ExecutionPolicy& policy, std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindAllDocuments(const ExecutionPolicy& policy, const Query& query, DocumentPredicate document_predicate) const;

    static bool IsValidWord(std::string_view word);
    bool IsStopWord(std::string_view word) const;
};

template <typename ExecutionPolicy>
SearchServer::Query SearchServer::ParseQuery(const ExecutionPolicy& policy, std::string_view text) const{
    Query query = {};
    for (const auto& word : SplitIntoWordsStringView(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            } else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }

    std::sort(policy, query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(std::unique(policy, query.minus_words.begin(), query.minus_words.end()), query.minus_words.end());

    std::sort(policy, query.plus_words.begin(), query.plus_words.end());
    query.plus_words.erase(std::unique(policy, query.plus_words.begin(), query.plus_words.end()), query.plus_words.end());

    return query;
}


template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus status) const{
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus statusp, int rating) { return statusp == status; });
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const{
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const{
    const auto query = ParseQuery(raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(policy, query, document_predicate);

    std::sort(policy, matched_documents.begin(), matched_documents.end(),
              [](const Document& lhs, const Document& rhs) {
                  if (std::fabs(lhs.relevance - rhs.relevance) < EPSILON) {
                      return lhs.rating > rhs.rating;
                  } else {
                      return lhs.relevance > rhs.relevance;
                  }
              });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

/* FIND ALL DOCUMENTS*/
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy& policy, const Query& query, DocumentPredicate document_predicate) const{
    ConcurrentMap<int, double> document_to_relevance(128);

    auto plus_words_proc = [this, &document_to_relevance, &document_predicate](std::string_view word){
        const auto itr = word_to_document_freqs_.find(word);
        if (itr != word_to_document_freqs_.cend()){
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : itr->second){
                const auto& [rating, status] = documents_.at(document_id);
                if (document_predicate(document_id, status, rating)){
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
    };
    std::for_each(policy, query.plus_words.begin(), query.plus_words.end(), plus_words_proc);

    auto minus_words_proc = [this, &document_to_relevance](std::string_view word){
        const auto itr = word_to_document_freqs_.find(word);
        if (itr != word_to_document_freqs_.cend()){
            for (const auto [document_id, _] : itr->second){
                document_to_relevance.Erase(document_id);
            }
        }
    };
    std::for_each(policy, query.minus_words.begin(), query.minus_words.end(), minus_words_proc);

    auto document_to_relevance_ordinary = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    matched_documents.reserve(document_to_relevance_ordinary.size());
    for (const auto [document_id, relevance] : document_to_relevance_ordinary) {
        matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
    }
    return matched_documents;
}
