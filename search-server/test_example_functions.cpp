#include "test_example_functions.h"
//#include "remove_duplicates.h"
#include "request_queue.h"
#include "process_queries.h"
#include <execution>
#include <vector>

std::string GenerateWord(std::mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    std::string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}

std::vector<std::string> GenerateDictionary(std::mt19937& generator, int word_count, int max_length) {
    std::vector<std::string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

std::string GenerateQuery(std::mt19937& generator, const std::vector<std::string>& dictionary, int word_count, double minus_prob) {
    std::string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

std::vector<std::string> GenerateQueries(std::mt19937& generator, const std::vector<std::string>& dictionary, int query_count, int max_word_count) {
    std::vector<std::string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

using namespace std;
void PrintMatchDocumentResultUTestOver(int document_id, const std::vector<std::string_view>& words, DocumentStatus status) {
    std::cout << "{ "
              << "document_id = " << document_id << ", "
              << "status = " << static_cast<int>(status) << ", "
              << "words =";
    for (std::string_view word : words) {
        std::cout << ' ' << word;
    }
    std::cout << "}" << std::endl;
}

void PrintDocumentUTestOver(const Document& document) {
    std::cout << "{ "
              << "document_id = " << document.id << ", "
              << "relevance = " << document.relevance << ", "
              << "rating = " << document.rating << " }" << std::endl;
}

void Test(string_view mark, const SearchServer& search_server, const vector<string>& queries) {
    LOG_DURATION(mark);
    double total_relevance = 0;
    for (const string_view query : queries) {
        for (const auto& document : search_server.FindTopDocuments(query)) {
            total_relevance += document.relevance;
        }
    }
    cout << total_relevance << endl;
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
    RUN_TEST(TestGetWordFrequencies);
    RUN_TEST(TestRemoveDocument);
    //RUN_TEST(TestRemoveDuplicates);
    RUN_TEST(TestRequestQueue);
    RUN_TEST(TestParProcessQueries);
    RUN_TEST(TestParProcessQueriesJoined);
    RUN_TEST(TestParallelRemoveDoc);
    RUN_TEST(TestParallelMatchDoc);
    RUN_TEST(Test7ProblemWithMatch);
    RUN_TEST(TestPerformanceParallelMatching);
    //RUN_TEST(TestParallelFindTopDocuments);
    //RUN_TEST(TestPerformanceParallelFindTop);
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city";
    const std::vector<int> ratings = {1, 2, 3};
    {
        SearchServer server("and in at"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in").empty(),
                    "Stop words must be excluded from documents");
    }
   //случай без стоп-слов
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(!server.FindTopDocuments("in").empty(),
                    "Words \"in\" must not be excluded from documents");
    }
}

// Тест проверяет, что документы добавляются в систему
void TestAddingDocuments(){

    SearchServer server("на в и"s);
    // Проверим, что до добавления докуметов система пустая
    ASSERT_EQUAL(server.GetDocumentCount(), 0);

    //Добавили в систему 4 документа
    server.AddDocument(0, "белый кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(2, "пушистый кот пушистый хвост",       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза", DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений",         DocumentStatus::BANNED, {9});

    // Проверим, что их правильное количество
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 4, "Wrong number of documents");

    //Проверим, что добавленные документы находятся по запросу со словами из документа
    {
        const auto found_docs = server.FindTopDocuments("кот");
        ASSERT_EQUAL_HINT(found_docs.size(), 2, "Wrong number of documents found");
        // обратный порядок из-за сортировки
        ASSERT_EQUAL(found_docs[1].id, 0);
        ASSERT_EQUAL(found_docs[0].id, 2);
    }
    //Проверим, что добавленные, но забаненные документы, не находятся по запросу
    ASSERT_HINT(server.FindTopDocuments("скворец").empty(), "Banned documents should not be on request");

}

// Тест проверяет, что система исключает документы с минус-словами из выдачи
void TestExcludeMinusWordsFromAddedDocumentContent(){
     SearchServer server("на в и"s);
     //Добавили в систему 3 документа
     server.AddDocument(0, "белый кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
     server.AddDocument(2, "пушистый кот пушистый хвост",       DocumentStatus::ACTUAL, {7, 2, 7});
     server.AddDocument(1, "ухоженный пёс выразительные глаза", DocumentStatus::ACTUAL, {5, -12, 2, 1});

     // Сначала убеждаемся, что поиск слов, не входящих в список минус-слов,находит все нужные документы
     {
         const auto found_docs = server.FindTopDocuments( "кот глаза");
         ASSERT_EQUAL_HINT(found_docs.size(), 3, "Wrong number of documents");
     }

     //Затем убеждаемся, что поиск этих же слов, но с добавлением минус-слова, возвращает корректный результат
     {
         const auto found_docs = server.FindTopDocuments("кот глаза -хвост");
         ASSERT_EQUAL_HINT(found_docs.size(), 2, "Wrong number of documents");
     }
}

//Тест проверяет матчинг документов
void TestMatchingDocuments(){
    SearchServer server("на в и"s);
    //Добавили в систему документ
    server.AddDocument(0, "белый кот и модный ошейник", DocumentStatus::ACTUAL, {8, -3});
    // Сначала убеждаемся, что при матчинге документа вернутся все слова из поискового запроса, присутствующие в документе.
    {
        auto [matched_word, _] = server.MatchDocument("кот ошейник", 0);
        ASSERT_EQUAL(matched_word.size(), 2);
        ASSERT_EQUAL(matched_word[0], "кот");
        ASSERT_EQUAL(matched_word[1], "ошейник");
    }

    //Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
    {
        auto [matched_word, _] = server.MatchDocument("кот ошейник -белый", 0);
        ASSERT_EQUAL_HINT(matched_word.size(), 0, "Should return an empty list of words");
        ASSERT(matched_word.empty());
    }

    {
        //SearchServer server("������"s);
        //auto [matched_word, _] = server.MatchDocument("������ -", 0);
    }
}

//Тест вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestDocumentRatingCalculation(){
    SearchServer server("на в и"s);
    //Добавили в систему 2 документа
    server.AddDocument(0, "белый кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -4});
    server.AddDocument(3, "ухоженный скворец евгений",         DocumentStatus::ACTUAL, {9});
    server.AddDocument(1, "кот первые мстители весна аутоимунное заболевание",  DocumentStatus::ACTUAL, {});
    const auto found_docs = server.FindTopDocuments("кот скворец");
    ASSERT_EQUAL(found_docs[0].rating, 9);
    ASSERT_EQUAL(found_docs[1].rating, 2);
    ASSERT_EQUAL(found_docs[2].rating, 0);
}

// Тест корректности рассчета релевантностей и сортировки найденных документов по убыванию релевантности
void TestForSortingFoundDocumentsInDescendingOrderOfRelevance(){
    SearchServer server("на в и"s);
    // Убедимся, что поиск на пустой системе ничего не даст
    {
        const auto found_docs = server.FindTopDocuments("кот глаза");
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Searching on an empty system should return nothing");
    }
    //Добавили в систему 4 документа
    server.AddDocument(0, "белый кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(2, "пушистый кот пушистый хвост",       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза", DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений",         DocumentStatus::ACTUAL, {9});

    //Убедимся, что поиск пустой строки ничего не даст
    {
        const auto found_docs = server.FindTopDocuments("");
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Searching for an empty string should return nothing");
    }
    // Проверка для случая, когда все релевантности отличаются на значимую величину
    {
        const auto found_docs = server.FindTopDocuments("кот глаза");
        ASSERT((abs(found_docs[0].relevance - 0.346574) < EPSILON));
        ASSERT((abs(found_docs[1].relevance - 0.173287) < EPSILON));
        ASSERT((abs(found_docs[2].relevance - 0.173287) < EPSILON));
    }
    // Проверка для сортировки с учетом рейтинга
    {
        server.AddDocument(4, "белый кот и модный ошейник", DocumentStatus::ACTUAL, {8, -2});
        const auto found_docs = server.FindTopDocuments("кот глаза");
        ASSERT((found_docs[2].rating > found_docs[3].rating) && (found_docs[2].id == 4));
    }
}

// Тест фильтрации результатов поиска с использованием предиката, задаваемого пользователем.
void TestFilteringSearchResultsUsingAUserSpecifiedPredicate(){
    SearchServer server("на в и"s);
    //Добавили в систему 3 документа
    server.AddDocument(0, "белый кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(2, "пушистый кот пушистый хвост",       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза", DocumentStatus::ACTUAL, {5, -12, 2, 1});
    // Проверяем на работу с предикатом по поиску нечетных индексов
    {
        const auto found_docs = server.FindTopDocuments("кот глаза",[](int document_id, DocumentStatus status, int rating)
            { return document_id % 2 != 0; });
        ASSERT_EQUAL(found_docs.size(), 1);
    }

}

//Тест поиска документов, имеющих заданный статус.
void TestForFindingDocumentsWithAGivenStatus(){
    SearchServer server("на в и"s);
    //Добавили в систему 3 документа
    server.AddDocument(0, "белый кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(2, "пушистый кот пушистый хвост",       DocumentStatus::BANNED, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза", DocumentStatus::BANNED, {5, -12, 2, 1});
    // Ищем документы с определенным статусом
    {
        const auto found_docs = server.FindTopDocuments("кот глаза", DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 2);
    }
    // Ищем документы со статусом по умолчанию
    {
        const auto found_docs = server.FindTopDocuments("кот глаза");
        ASSERT_EQUAL(found_docs.size(), 1);
    }
    // Ищем документы со статусом, документов с которым нет
    {
        const auto found_docs = server.FindTopDocuments("кот глаза", DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Documents with this status should not be");
    }
}

void TestGetWordFrequencies(){
    SearchServer server("на в и"s);
    //Добавили в систему 3 документа
    server.AddDocument(0, "белый кот кот кот кот кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(2, "пушистый кот пушистый хвост",       DocumentStatus::BANNED, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза", DocumentStatus::BANNED, {5, -12, 2, 1});

    // если такого документа нет
    {
        std::map<std::string, double> res_s;
        auto res = server.GetWordFrequencies(3);
        ASSERT_EQUAL_HINT(res.size(), res_s.size(), "No such document exists");
    }
    // базовая ситуация
    {
        auto res = server.GetWordFrequencies(0);
        ASSERT_EQUAL_HINT(res.size(), 4 , "Incorrect number of words");
        ASSERT_EQUAL_HINT(res.at("модный"), 0.125, "Incorrect word frequency");
    }
    // повторная проверка (смотреть отладчиком, в какую ветку входит
    {
        auto res = server.GetWordFrequencies(1);
        ASSERT_EQUAL_HINT(res.size(), 4 , "Incorrect number of words");
    }
}

//Тест удаления документов
void TestRemoveDocument(){
    SearchServer server("на в и"s);
    //Добавили в систему 3 документа
    server.AddDocument(0, "белый кот кот кот кот кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(2, "пушистый кот пушистый хвост",       DocumentStatus::BANNED, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза", DocumentStatus::BANNED, {5, -12, 2, 1});

    // базовая ситуация
    {
        server.RemoveDocument(0);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 2, "Incorrect remove");
    }
}

//Тест удаления дубликатов
/*void TestRemoveDuplicates(){
    SearchServer server("на в и"s);
    //Добавили в систему 3 документа
    server.AddDocument(0, "белый кот кот кот кот кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(2, "пушистый кот пушистый хвост",       DocumentStatus::BANNED, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза", DocumentStatus::BANNED, {5, -12, 2, 1});

    // если дубликатов нет
    {
        RemoveDuplicates(server);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 3, "If there was no duplicates, no document should have been removed");
    }
    //Добавили в систему 3 документа
    server.AddDocument(3, "белый кот кот кот кот кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(4, "белый кот кот кот кот кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(5, "белый кот кот кот кот кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});

    // базовая ситуация
    {
        ASSERT_EQUAL(server.GetDocumentCount(), 6);
        RemoveDuplicates(server);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 3, "Incorrect dublicates remove");
    }

}*/

//Тест очереди запросов
void TestRequestQueue(){
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
    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 1437);
}





//Тест распараллеливания обработки нескольких запросов к поисковой системе
void TestParProcessQueries(){
    SearchServer search_server("and with"s);
    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
        ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }
    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };
    auto documents = ProcessQueries(search_server, queries);
    ASSERT_EQUAL(documents[0].size(), 3);
    ASSERT_EQUAL(documents[1].size(), 5);
    ASSERT_EQUAL(documents[2].size(), 2);
}

void TestParProcessQueriesJoined(){
    SearchServer search_server("and with"s);
    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
        ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }
    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };
    auto documents =  ProcessQueriesJoined(search_server, queries);

    auto it = documents.begin();
    ASSERT_EQUAL((*it).id, 1);
    ASSERT(fabs((*it).relevance - 0.183492) < EPSILON);
    it = std::next(it, 4);
    ASSERT_EQUAL((*it).id, 1);
    ASSERT(fabs((*it).relevance - 0.311199) < EPSILON);
    it = std::next(it, 4);
    ASSERT_EQUAL((*it).id, 2);
    ASSERT(fabs((*it).relevance - 0.458145) < EPSILON);
}

void TestParallelRemoveDoc(){
    SearchServer search_server("and with"s);
    {
        int id = 0;
        for (
            const string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
            }
            ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
        }
    }
    const string query = "curly and funny"s;

    auto check = [&search_server, &query](int total, int forQ) {
        ASSERT_EQUAL(search_server.GetDocumentCount(), total);
        ASSERT_EQUAL(search_server.FindTopDocuments(query).size(), forQ);
    };

    check(5, 4);
    // однопоточная версия
    search_server.RemoveDocument(5);
    check(4, 3);
    // однопоточная версия
    search_server.RemoveDocument(std::execution::seq, 1);
    check(3, 2);
    // многопоточная версия
    search_server.RemoveDocument(std::execution::par, 2);
    check(2, 1);
}

void TestParallelMatchDoc(){
    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
        ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const string query = "curly and funny -not"s;

    {
        const auto [words, status] = search_server.MatchDocument(query, 1);
        ASSERT_EQUAL(words.size(), 1);
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::par, query, 2);
        ASSERT_EQUAL(words.size(), 2);
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
        ASSERT_EQUAL(words.size(), 0);
    }
}

void Test7ProblemWithMatch(){
    const std::vector<int> ratings1 = {1, 2, 3, 4, 5};
    const std::vector<int> ratings2 = {-1, -2, 30, -3, 44, 5};
    const std::vector<int> ratings3 = {12, -20, 80, 0, 8, 0, 0, 9, 67};
    std::string stop_words = "и в на";
    SearchServer search_server(stop_words);

    search_server.AddDocument(0, "белый кот и модный ошейник", DocumentStatus::ACTUAL, ratings1);
    search_server.AddDocument(1, "пушистый кот пушистый хвост", DocumentStatus::ACTUAL, ratings2);
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза", DocumentStatus::ACTUAL,
                              ratings3);
    search_server.AddDocument(3, "белый модный кот", DocumentStatus::IRRELEVANT, ratings1);
    search_server.AddDocument(4, "пушистый кот пёс", DocumentStatus::IRRELEVANT, ratings2);
    search_server.AddDocument(5, "ухоженный ошейник выразительные глаза",
                              DocumentStatus::IRRELEVANT, ratings3);
    search_server.AddDocument(6, "кот и ошейник", DocumentStatus::BANNED, ratings1);
    search_server.AddDocument(7, "пёс и хвост", DocumentStatus::BANNED, ratings2);
    search_server.AddDocument(8, "модный пёс пушистый хвост", DocumentStatus::BANNED, ratings3);
    search_server.AddDocument(9, "кот пушистый ошейник", DocumentStatus::REMOVED, ratings1);
    search_server.AddDocument(10, "ухоженный кот и пёс", DocumentStatus::REMOVED, ratings2);
    search_server.AddDocument(11, "хвост и выразительные глаза", DocumentStatus::REMOVED, ratings3);

    const std::string query = "пушистый ухоженный кот -ошейник";
    const auto documents = search_server.FindTopDocuments(query);

    std::cout << "Top documents for query:" << std::endl;
    for (const Document& document : documents) {
        PrintDocumentUTestOver(document);
    }

    std::cout << "Documents' statuses:" << std::endl;
    const int document_count = search_server.GetDocumentCount();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        const auto [words, status] = search_server.MatchDocument(query, document_id);
        PrintMatchDocumentResultUTestOver(document_id, words, status);
    }

//Top documents for query:
//{ document_id = 1, relevance = 0.684055, rating = 12 }
//{ document_id = 2, relevance = 0.346574, rating = 17 }
//Documents' statuses:
//{ document_id = 0, status = 0, words =}
//{ document_id = 1, status = 0, words = кот пушистый}
//{ document_id = 2, status = 0, words = ухоженный}
//{ document_id = 3, status = 1, words = кот}
//{ document_id = 4, status = 1, words = кот пушистый}
//{ document_id = 5, status = 1, words =}
//{ document_id = 6, status = 2, words =}
//{ document_id = 7, status = 2, words =}
//{ document_id = 8, status = 2, words = пушистый}
//{ document_id = 9, status = 3, words =}
//{ document_id = 10, status = 3, words = кот ухоженный}
//{ document_id = 11, status = 3, words =}

}

void TestParallelFindTopDocuments(){
    SearchServer search_server("and with"s);
    int id = 0;
    for (
        const string& text : {
            "white cat and yellow hat"s,
            "curly cat curly tail"s,
            "nasty dog with big eyes"s,
            "nasty pigeon john"s,
        }
        ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    cout << "ACTUAL by default:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
        PrintDocumentUTestOver(document);
    }
    cout << "BANNED:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s, DocumentStatus::BANNED)) {
        PrintDocumentUTestOver(document);
    }
    cout << "Even ids:"s << endl;
    // параллельная версия
    for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocumentUTestOver(document);
    }



//    cout << "PACTUAL by default:"s << endl;
//    // последовательная версия
//    for (const Document& document : search_server.FindTopDocuments(execution::par,"curly nasty cat"s)) {
//        PrintDocumentUTestOver(document);
//    }
//    cout << "PBANNED:"s << endl;
//    // последовательная версия
//    for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, DocumentStatus::BANNED)) {
//        PrintDocumentUTestOver(document);
//    }
//    cout << "PEven ids:"s << endl;
//    // параллельная версия
////    //for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
////        PrintDocumentUTestOver(document);
////    }
    ////    ACTUAL by default:
    ////    { document_id = 2, relevance = 0.866434, rating = 1 }
    ////    { document_id = 4, relevance = 0.231049, rating = 1 }
    ////    { document_id = 1, relevance = 0.173287, rating = 1 }
    ////    { document_id = 3, relevance = 0.173287, rating = 1 }
    ////     BANNED:
    ////    Even ids:
    ////    { document_id = 2, relevance = 0.866434, rating = 1 }
    ////    { document_id = 4, relevance = 0.231049, rating = 1 }

}

void TestPerformanceParallelMatching(){
    mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 1000, 10);
    const auto documents = GenerateQueries(generator, dictionary, 10'00, 70);
    const string query = GenerateQuery(generator, dictionary, 500, 0.1);
    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }
    TESTMATCDOCPERF(seq);
    TESTMATCDOCPERF(par);
}

void TestPerformanceParallelFindTop(){
    mt19937 generator;
    const auto dictionary = GenerateDictionary(generator, 1000, 10);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);
    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }
    const auto queries = GenerateQueries(generator, dictionary, 100, 70);
    TEST(seq);
    //TEST(par);
    Test("no policy", search_server, queries);
}
