#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <numeric>

using namespace std;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "Assert("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Func>
void RunTestImpl(const Func& f, const string& f_str) {
         f();
         cerr << f_str << " OK"s << endl;
}

#define RUN_TEST(func)  RunTestImpl((func), #func)

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
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL ) const {
        return FindTopDocuments(raw_query, [status](int, DocumentStatus doc_status, int) {
            return doc_status == status;
        });
    }

    template <typename DocFiltr>
    vector<Document> FindTopDocuments(const string& raw_query, DocFiltr doc_filtr) const {
        const Query query = ParseQuery(raw_query);
        vector<Document> matched_documents = FindAllDocuments(query, doc_filtr);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < EPSILON) return lhs.rating > rhs.rating;
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,int document_id) const {
        const Query query = ParseQuery(raw_query);
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
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
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
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
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

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocFilt>
    vector<Document> FindAllDocuments(const Query& query, DocFilt doc_filt) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
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
            auto rating = documents_.at(document_id).rating;
            if (doc_filt(document_id, documents_.at(document_id).status, rating))
               matched_documents.push_back({document_id, relevance, rating});
        }
        return matched_documents;
    }
};

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет, что документы добавляются в систему
void TestAddingDocuments(){
    SearchServer search_server;
    // Проверим, что до добавления докуметов система пустая
    ASSERT_EQUAL(search_server.GetDocumentCount(), 0);

    //Добавили в систему 4 документа
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(2, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(1, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    // Проверим, что их правильное количество
    ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 4, "Wrong number of documents"s);

    //Проверим, что добавленные документы находятся по запросу со словами из документа
    {
        const auto found_docs = search_server.FindTopDocuments("кот"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 2, "Wrong number of documents found"s);
        // обратный порядок из-за сортировки
        ASSERT_EQUAL(found_docs[1].id, 0);
        ASSERT_EQUAL(found_docs[0].id, 2);
    }
    //Проверим, что добавленные, но забаненные документы, не находятся по запросу
    ASSERT_HINT(search_server.FindTopDocuments("скворец"s).empty(), "Banned documents should not be on request"s);

}

// Тест проверяет, что система исключает документы с минус-словами из выдачи
void TestExcludeMinusWordsFromAddedDocumentContent(){
     SearchServer search_server;
     //Добавили в систему 3 документа
     search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
     search_server.AddDocument(2, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
     search_server.AddDocument(1, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});

     // Сначала убеждаемся, что поиск слов, не входящих в список минус-слов,находит все нужные документы
     {
         const auto found_docs = search_server.FindTopDocuments("кот глаза"s);
         ASSERT_EQUAL_HINT(found_docs.size(), 3, "Wrong number of documents"s);
     }

     //Затем убеждаемся, что поиск этих же слов, но с добавлением минус-слова, возвращает корректный результат
     {
         const auto found_docs = search_server.FindTopDocuments("кот глаза -хвост"s);
         ASSERT_EQUAL_HINT(found_docs.size(), 2, "Wrong number of documents"s);
     }
}

//Тест проверяет матчинг документов
void TestMatchingDocuments(){
    SearchServer search_server;
    //Добавили в систему документ
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    // Сначала убеждаемся, что при матчинге документа вернутся все слова из поискового запроса, присутствующие в документе.
    {
        auto [matched_word, _] = search_server.MatchDocument("кот ошейник", 0);
        ASSERT_EQUAL(matched_word.size(), 2);
        ASSERT_EQUAL(matched_word[0], "кот"s);
        ASSERT_EQUAL(matched_word[1], "ошейник"s);
    }

    //Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
    {
        auto [matched_word, _] = search_server.MatchDocument("кот ошейник -белый", 0);
        ASSERT_EQUAL_HINT(matched_word.size(), 0, "Should return an empty list of words"s);
    }

    //Проверка корректной обработки пустого минус-слова
    {
        auto [matched_word, _] = search_server.MatchDocument("кот ошейник -", 0);
        ASSERT_EQUAL(matched_word.size(), 2);
    }
}

//Тест вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestDocumentRatingCalculation(){
    SearchServer search_server;
    //Добавили в систему 2 документа
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -4});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::ACTUAL, {9});
    search_server.AddDocument(1, "кот первые мстители весна аутоимунное заболевание"s,  DocumentStatus::ACTUAL, {});
    const auto found_docs = search_server.FindTopDocuments("кот скворец"s);
    ASSERT_EQUAL(found_docs[0].rating, 9);
    ASSERT_EQUAL(found_docs[1].rating, 2);
    ASSERT_EQUAL(found_docs[2].rating, 0);
}

// Тест корректности рассчета релевантностей и сортировки найденных документов по убыванию релевантности
void TestForSortingFoundDocumentsInDescendingOrderOfRelevance(){
    SearchServer search_server;
    // Убедимся, что поиск на пустой системе ничего не даст
    {
        const auto found_docs = search_server.FindTopDocuments("кот глаза"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Searching on an empty system should return nothing"s);
    }
    //Добавили в систему 4 документа
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(2, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(1, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::ACTUAL, {9});

    //Убедимся, что поиск пустой строки ничего не даст
    {
        const auto found_docs = search_server.FindTopDocuments(""s);
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Searching for an empty string should return nothing"s);
    }
    // Проверка для случая, когда все релевантности отличаются на значимую величину
    {
        const auto found_docs = search_server.FindTopDocuments("кот глаза"s);
        ASSERT((abs(found_docs[0].relevance - 0.346574) < EPSILON));
        ASSERT((abs(found_docs[1].relevance - 0.173287) < EPSILON));
        ASSERT((abs(found_docs[2].relevance - 0.138629) < EPSILON));
    }
    // Проверка для сортировки с учетом рейтинга
    {
        search_server.AddDocument(4, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -2});
        const auto found_docs = search_server.FindTopDocuments("кот глаза"s);
        ASSERT((found_docs[2].rating > found_docs[3].rating) && (found_docs[2].id == 4));
    }
}

// Тест фильтрации результатов поиска с использованием предиката, задаваемого пользователем.
void TestFilteringSearchResultsUsingAUserSpecifiedPredicate(){
    SearchServer search_server;
    //Добавили в систему 3 документа
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(2, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(1, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    // Проверяем на работу с предикатом по поиску нечетных индексов
    {
        const auto found_docs = search_server.FindTopDocuments("кот глаза"s,[](int document_id, DocumentStatus status, int rating)
            { return document_id % 2 != 0; });
        ASSERT_EQUAL(found_docs.size(), 1);
    }

}

//Тест поиска документов, имеющих заданный статус.
void TestForFindingDocumentsWithAGivenStatus(){
    SearchServer search_server;
    //Добавили в систему 3 документа
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(2, "пушистый кот пушистый хвост"s,       DocumentStatus::BANNED, {7, 2, 7});
    search_server.AddDocument(1, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, {5, -12, 2, 1});
    // Ищем документы с определенным статусом
    {
        const auto found_docs = search_server.FindTopDocuments("кот глаза"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 2);
    }
    // Ищем документы со статусом по умолчанию
    {
        const auto found_docs = search_server.FindTopDocuments("кот глаза"s);
        ASSERT_EQUAL(found_docs.size(), 1);
    }
    // Ищем документы со статусом, документов с которым нет
    {
        const auto found_docs = search_server.FindTopDocuments("кот глаза"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Documents with this status should not be"s);
    }
}


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddingDocuments);
    RUN_TEST(TestExcludeMinusWordsFromAddedDocumentContent);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestDocumentRatingCalculation);
    RUN_TEST(TestForSortingFoundDocumentsInDescendingOrderOfRelevance);
    RUN_TEST(TestFilteringSearchResultsUsingAUserSpecifiedPredicate);
    RUN_TEST(TestForFindingDocumentsWithAGivenStatus);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
