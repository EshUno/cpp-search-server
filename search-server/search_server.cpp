#include "search_server.h"
#include <numeric>

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) throw std::invalid_argument("Документ с отрицательным ID");
    if (documents_.count(document_id)) throw std::invalid_argument("Документ с повторным ID");
    const std::vector<std::string> words = SplitIntoWordsNoStop(document);
    for (const auto& word : words){
        if (!IsValidWord(word)) throw std::invalid_argument("Недопустимый формат слов");
    }
    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    ids_.emplace(document_id);
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status, words});
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query,  [status](int , DocumentStatus document_status, int ) {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
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

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return std::tuple<std::vector<std::string>, DocumentStatus>{matched_words, documents_.at(document_id).status};
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const{
    // если для этого документа уже существует структура с частотами
    {
        auto already_ex = word_freqs_.find(document_id);
        if (already_ex  != word_freqs_.end()) return already_ex->second;
    }

    {
        auto doc = documents_.find(document_id);
        if ( doc != documents_.end()){
            std::map<std::string, double> res;
            for (auto &x : doc->second.words){
                if (res.count(x) == 0){
                    double freq = word_to_document_freqs_.at(x).at(document_id);
                    res.emplace(x, freq);
                }
            }
            word_freqs_.emplace(std::make_pair(document_id, res));
            return word_freqs_.at(document_id);
        }
    }

    // если такого документа нет
    {
        static std::map<std::string, double> res_s;
        return res_s;
    }
}

void SearchServer::RemoveDocument(int document_id){
    if (ids_.count(document_id)){
        ids_.erase(document_id);
        word_freqs_.erase(document_id);

        std::vector<std::string> res = documents_.at(document_id).words;
        for (auto &w : res)
        {
            auto itr = word_to_document_freqs_.find(w);
            if (itr != word_to_document_freqs_.end())
            {
                auto& doc_to_freq = itr->second;
                doc_to_freq.erase(document_id);
                if (doc_to_freq.size() == 0)
                {
                    word_to_document_freqs_.erase(itr);
                }
            }
        }

        documents_.erase(document_id);
    }
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
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

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    for (const std::string& word : SplitIntoWords(text)) {
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

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

 bool SearchServer::IsValidWord(const std::string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}
