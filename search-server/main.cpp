#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
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
};

struct Query{
       set<string> plus_words;
       set<string> minus_words;
       void PrintWords() const{
           cout << "plus: " << endl;
           for (auto x: plus_words)
               cout << x << "  ";
           cout<< endl << "minus: " << endl;
           for (auto x: minus_words)
               cout << x << "  ";
           
       }
    };

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        ++document_count_;
        double frequency = 1. / words.size();
        for (auto word : words){       
           indices_[word][document_id] += frequency;
            
        }     
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:    
    map <string, map<int, double>> indices_;
    set<string> stop_words_;
    int document_count_ = 0;

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

    Query ParseQuery(const string& text) const {
        Query query_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-') query_words.minus_words.insert(word.substr(1));
            else query_words.plus_words.insert(word);
        }
        return query_words;
    }

    vector<Document> FindAllDocuments(const Query& query_words) const {
        vector<Document> matched_documents;
        map<int, double> match;
        MatchDoc(match, query_words);
        for (auto doc: match ){
            matched_documents.push_back({doc.first, doc.second});
        }
        return matched_documents;
    }   
    
    double CalcIdf(const string &word) const{
        return log(document_count_ * 1.0 / indices_.at(word).size());
    }
       
    void PlusMatch(map<int, double>& match, const  set<string>& query_plus) const {
        for(auto word: query_plus){
            if (indices_.count(word) != 0){
                double idf = CalcIdf(word);
                for (const auto [id, tf]: indices_.at(word))
                    match[id] += tf * idf;
            }   
        }
    }
                                             
    void MinusMatch(map<int, double>& match, const  set<string>& query_minus) const{
        for (auto word: query_minus) {
            if (indices_.count(word) != 0){
                for (const auto [id, T_]: indices_.at(word))
                    match.erase(id);
            }   
        }
    }
    
    void MatchDoc(map<int, double>& match, const Query& query_words) const{
        PlusMatch(match, query_words.plus_words);
        MinusMatch(match, query_words.minus_words);
        
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
        for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}
