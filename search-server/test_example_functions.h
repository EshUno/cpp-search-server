#pragma once
#include <string>
#include <iostream>

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

void TestRemoveDocument();

void TestRemoveDuplicates();
