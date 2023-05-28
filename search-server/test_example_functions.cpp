#include "test_example_functions.h"
#include <vector>
#include "search_server.h"
#include "remove_duplicates.h"
#include <string>
using namespace std;
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
    RUN_TEST(TestRemoveDuplicates);
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
         const auto found_docs = server.FindTopDocuments("кот глаза");
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

void TestRemoveDocument(){
    SearchServer server("на в и"s);
    //Добавили в систему 3 документа
    server.AddDocument(0, "белый кот кот кот кот кот и модный ошейник",        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(2, "пушистый кот пушистый хвост",       DocumentStatus::BANNED, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза", DocumentStatus::BANNED, {5, -12, 2, 1});

    // если такого документа нет
    {
        server.RemoveDocument(3);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 3, "No such document exists");
    }

    // базовая ситуация
    {
        server.RemoveDocument(0);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 2, "Incorrect remove");
    }
}

void TestRemoveDuplicates(){
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

}
