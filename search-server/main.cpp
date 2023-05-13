#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <deque>

using namespace std;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

ostream& operator<<(ostream &os, const Document& doc) {
    return os << "{ "s
              << "document_id = "s << doc.id << ", "s
              << "relevance = "s << doc.relevance << ", "s
              << "rating = "s << doc.rating
              << " }"s;
}


template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

template <typename Iterator>
class IteratorRange{
    pair<Iterator, Iterator> page;

public:
    Iterator begin() const {
        return page.first;
    }
    Iterator end() const {
        return page.second;
    }
    size_t size() const{
        return distance(page.begin, page.end);
    }
    IteratorRange(Iterator begin, Iterator end) : page{begin, end}{
    }
};

template <typename Iterator>
ostream& operator<<(ostream &os, const IteratorRange<Iterator>& rng) {
    for (auto i = rng.begin(); i != rng.end(); ++i){
        os << *i ;
    }
    return os;
}

template <typename Iterator>
class Paginator{
    vector<IteratorRange<Iterator>> pages;

public:
    auto begin() const{
        return pages.begin();
    }

    auto end() const{
        return pages.end();
    }
    Paginator(Iterator begin, Iterator end, size_t page_size){
        size_t dist = distance(begin, end);
        size_t size = dist / page_size;
        while(size){
            Iterator new_end = begin;
            advance(new_end, page_size);
            pages.push_back(IteratorRange(begin, new_end));
            advance(begin, page_size);
            --size;
        }
        auto ost = dist % page_size;
        if (ost > 0) {
            Iterator new_end = begin;
            advance(new_end, ost);
            pages.push_back(IteratorRange(begin, new_end));
        }
    }
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

class SearchServer {
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        for (const auto& word : stop_words_){
            if (!IsValidWord(word)) throw invalid_argument("Недопустимый формат слов"s);
        }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0) throw invalid_argument("Документ с отрицательным ID"s);
        if (documents_.count(document_id)) throw invalid_argument("Документ с повторным ID"s);
        const vector<string> words = SplitIntoWordsNoStop(document);
        for (const auto& word : words){
            if (!IsValidWord(word)) throw invalid_argument("Недопустимый формат слов"s);
        }
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);
        vector<Document> matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query,  [status](int , DocumentStatus document_status, int ) {
            return document_status == status;
        });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    int GetDocumentId(int index) const {
        if (index < 0 || index > documents_.size()) throw out_of_range("Индекс переходит за допустимый диапазон"s);
        auto it = documents_.begin();
        advance(it, index);
        return it->first;
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const auto query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return tuple<vector<string>, DocumentStatus>{matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        if (text.empty()) throw invalid_argument("Пустой поисковый запрос"s);
        if (text[0] == '-') throw invalid_argument("Более одного минуса в поисковом запросе"s);
        if (!IsValidWord(text)) throw invalid_argument("В поисковом запросе встречаются недопустимые символы"s);
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                   continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }

    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }
};

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : server_(search_server) {

    }

    void CheckRequest(const vector<Document>& results){
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
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
        const auto search_results = server_.FindTopDocuments(raw_query, document_predicate);
        CheckRequest(search_results);
        return search_results;
    }
    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status) {
        const auto search_results = server_.FindTopDocuments(raw_query, status);
        CheckRequest(search_results);
        return search_results;
    }
    vector<Document> AddFindRequest(const string& raw_query) {
        const auto search_results = server_.FindTopDocuments(raw_query);
        CheckRequest(search_results);
        return search_results;
    }
    int GetNoResultRequests() const {
        return zero_result_;
    }
private:
    struct QueryResult {
        bool result_is_zero = false;
    };
    deque<QueryResult> requests_;
    int zero_result_ = 0;
    const static int min_in_day_ = 1440;
    const SearchServer& server_;
};

int main() {
    try {
        SearchServer search_server("and in at"s);
        RequestQueue request_queue(search_server);
        search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
        search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
        search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
        search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
        // 1439 запросов с нулевым результатом
        for (int i = 0; i < 1439; ++i) {
            request_queue.AddFindRequest("empty request"s);
        }
        // все еще 1439 запросов с нулевым результатом
        request_queue.AddFindRequest("curly dog"s);

        // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
        request_queue.AddFindRequest("big collar"s);
        // первый запрос удален, 1437 запросов с нулевым результатом
        request_queue.AddFindRequest("sparrow"s);
        cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
    } catch (...) {
        cout << "Ошибка в поисковом запросе!"s << endl;

    }
}
