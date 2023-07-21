#include "search_server.h"
#include <numeric>

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) throw std::invalid_argument("Документ с отрицательным ID");
    if (documents_.count(document_id)) throw std::invalid_argument("Документ с повторным ID");
    storage_.emplace_back(document);
    const std::vector<std::string_view> words = SplitIntoWordsNoStop(storage_.back());
    for (const auto word : words){
        if (!IsValidWord(word)) throw std::invalid_argument("Недопустимый формат слов");
    }
    const double inv_word_count = 1.0 / words.size();
    for (const auto word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_freqs_[document_id][word] += inv_word_count;
    }
    ids_.emplace(document_id);
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const{
    return FindTopDocuments(std::execution::seq, raw_query,  [status](int , DocumentStatus document_status, int ) {
        return document_status == status;
    });
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

int SearchServer::GetDocumentId(int index) const {
    if (index < 0 || index > documents_.size()) throw std::out_of_range("Индекс переходит за допустимый диапазон");
    auto it = documents_.begin();
    advance(it, index);
    return it->first;
}

std::set<int>::const_iterator SearchServer::begin(){
    return ids_.begin();
}

std::set<int>::const_iterator SearchServer::end(){
    return ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const{
    if (ids_.count(document_id)) {
        return word_freqs_.at(document_id);
    }
    static std::map<std::string_view, double> res_s{};
    return res_s;;
}

void SearchServer::RemoveDocument(int document_id){
    RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id){
    if (!ids_.count(document_id)) {
        throw std::out_of_range("Документа с указанным id не существует.");
    }
    for (const auto [word, _ ] :  GetWordFrequencies(document_id)) {
        word_to_document_freqs_[word].erase(document_id);
    }
    ids_.erase(document_id);
    word_freqs_.erase(document_id);
    documents_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id){
    if (!ids_.count(document_id)) {
        throw std::out_of_range("Документа с указанным id не существует.");
    }

    auto document_words = GetWordFrequencies(document_id);
    std::vector<std::string_view> words(document_words.size());
    std::transform(std::execution::par, document_words.begin(), document_words.end(),
                   words.begin(),
                   [](auto word){
                       return word.first;
                   });

    std::for_each(std::execution::par,
                  words.begin(), words.end(),
                  [this, document_id](auto word){
                      word_to_document_freqs_[word].erase(document_id);
                  });
    ids_.erase(document_id);
    word_freqs_.erase(document_id);
    documents_.erase(document_id);
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (auto word : SplitIntoWordsStringView(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    auto res =  MatchDocument(std::execution::seq, raw_query, document_id);
    return res;
}

std::tuple<std::vector<std::string_view>, DocumentStatus>  SearchServer::MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const{
    if (!ids_.count(document_id)) {
        throw std::out_of_range("Документа с указанным id не существует.");
    }
    const auto query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;

    for (const auto& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            return std::tuple<std::vector<std::string_view>, DocumentStatus> {std::move(matched_words), documents_.at(document_id).status};
        }
    }

    for (const auto& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(std::move(word));
        }
    }
    return std::tuple<std::vector<std::string_view>, DocumentStatus> {std::move(matched_words), documents_.at(document_id).status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus>  SearchServer::MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const{
    if (!ids_.count(document_id)) {
        throw std::out_of_range("Документа с указанным id не существует.");
    }
    const auto& query = ParseQuery(std::execution::par, raw_query);
    std::vector<std::string_view> matched_words;
    const auto word_freqs_doc = GetWordFrequencies(document_id);
    if (std::any_of(std::execution::par, query.minus_words.begin(),
                    query.minus_words.end(),
                    [&word_freqs_doc](const auto & word) {
                        return word_freqs_doc.count(word) > 0;
                    }))
        return std::tuple<std::vector<std::string_view>, DocumentStatus> {std::move(matched_words), documents_.at(document_id).status};

    matched_words.reserve(query.plus_words.size());
    std::copy_if(std::execution::par,query.plus_words.begin(),
                 query.plus_words.end(),
                 std::back_inserter(matched_words),
                 [&word_freqs_doc](const auto & word) {
                     return word_freqs_doc.count(word) > 0;
                 });

    std::sort(std::execution::par, matched_words.begin(), matched_words.end());
    auto last = std::unique(std::execution::par, matched_words.begin(), matched_words.end());
    matched_words.erase(last, matched_words.end());

    return std::tuple<std::vector<std::string_view>, DocumentStatus> {std::move(matched_words), documents_.at(document_id).status};
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text) const{
    return ParseQuery(std::execution::seq, text);
}

SearchServer::Query SearchServer::ParseQuery(std::execution::sequenced_policy, std::string_view text) const {
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

    std::sort(query.minus_words.begin(), query.minus_words.end());
    auto last_minus = std::unique(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(last_minus, query.minus_words.end());

    std::sort(query.plus_words.begin(), query.plus_words.end());
    auto last_plus = std::unique(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.erase(last_plus, query.plus_words.end());

    return query;
}

SearchServer::Query SearchServer::ParseQuery(std::execution::parallel_policy, std::string_view text) const {
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

    return query;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
    return ParseQueryWord(std::string_view{text});
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty()) throw std::invalid_argument("Пустой поисковый запрос");
    if (text[0] == '-') throw std::invalid_argument("Более одного минуса в поисковом запросе");
    if (!IsValidWord(text)) throw std::invalid_argument("В поисковом запросе встречаются недопустимые символы");
    return {text, is_minus, IsStopWord(text)};
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(std::string_view word) {
    return std::none_of(word.begin(), word.end(),
                        [](auto c) { return c >= '\0' && c < ' ';}
                        );
}

bool SearchServer::IsStopWord(std::string_view word) const{
    return (stop_words_.count(word) > 0);
}

