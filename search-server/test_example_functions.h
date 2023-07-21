#pragma once
#include <string>
#include <iostream>
#include <iterator>

#include <execution>
#include <random>
#include <string>
#include <vector>
#include "search_server.h"
#include "log_duration.h"
using namespace std;
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "(" << line << "): " << func << ": ";
        std::cerr << "ASSERT_EQUAL(" << t_str << ", " << u_str << ") failed: ";
        std::cerr << t << " != " << u << ".";
        if (!hint.empty()) {
            std::cerr << " Hint: " << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, "")
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

inline void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint) {
    if (!value) {
        std::cerr << file << "(" << line << "): " << func << ": ";
        std::cerr << "Assert(" << expr_str << ") failed.";
        if (!hint.empty()) {
            std::cerr << " Hint: " << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, "")
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Func>
void RunTestImpl(const Func& f, const std::string& f_str) {
    f();
    std::cerr << f_str << " OK" << std::endl;
}

#define RUN_TEST(func)  RunTestImpl((func), #func)

std::string GenerateWord(std::mt19937& generator, int max_length);

std::vector<std::string> GenerateDictionary(std::mt19937& generator, int word_count, int max_length);

std::string GenerateQuery(std::mt19937& generator, const std::vector<std::string>& dictionary, int word_count, double minus_prob = 0);

std::vector<std::string> GenerateQueries(std::mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count);

template <typename ExecutionPolicy>
void TestMatchDocPerf(std::string_view mark, SearchServer search_server, const std::string& query, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    const int document_count = search_server.GetDocumentCount();
    int word_count = 0;
    for (int id = 0; id < document_count; ++id) {
        const auto [words, status] = search_server.MatchDocument(policy, query, id);
        word_count += words.size();
    }
    std::cout << word_count << std::endl;
}

#define TESTMATCDOCPERF(policy) TestMatchDocPerf(#policy, search_server, query, execution::policy)

template <typename ExecutionPolicy>
void Test(string_view mark, const SearchServer& search_server, const vector<string>& queries, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    double total_relevance = 0;
    for (const string_view query : queries) {
        for (const auto& document : search_server.FindTopDocuments(policy, query)) {
            total_relevance += document.relevance;
        }
    }
    cout << total_relevance << endl;
}

void Test(string_view mark, const SearchServer& search_server, const vector<string>& queries);

#define TEST(policy) Test(#policy, search_server, queries, execution::policy)

void PrintMatchDocumentResultUTestOver(int document_id, const std::vector<std::string_view>& words, DocumentStatus status);
void PrintDocumentUTestOver(const Document& document);

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();
// Тест проверяет, что документы добавляются в систему
void TestAddingDocuments();
// Тест проверяет, что система исключает документы с минус-словами из выдачи
void TestExcludeMinusWordsFromAddedDocumentContent();
//Тест проверяет матчинг документов
void TestMatchingDocuments();
//Тест вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestDocumentRatingCalculation();
// Тест корректности рассчета релевантностей и сортировки найденных документов по убыванию релевантности
void TestForSortingFoundDocumentsInDescendingOrderOfRelevance();
// Тест фильтрации результатов поиска с использованием предиката, задаваемого пользователем.
void TestFilteringSearchResultsUsingAUserSpecifiedPredicate();
//Тест поиска документов, имеющих заданный статус.
void TestForFindingDocumentsWithAGivenStatus();

void TestGetWordFrequencies();
//Тест удаления документов
void TestRemoveDocument();
//Тест удаления дубликатов
//void TestRemoveDuplicates();
//Тест очереди запросов
void TestRequestQueue();
//Тест распараллеливания обработки нескольких запросов к поисковой системе
void TestParProcessQueries();
void TestParProcessQueriesJoined();
void TestParallelRemoveDoc();
void TestParallelMatchDoc();
void Test7ProblemWithMatch();
void TestPerformanceParallelMatching();

void TestParallelFindTopDocuments();
void TestPerformanceParallelFindTop();
