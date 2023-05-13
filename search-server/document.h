#pragma once
#ifndef DOCUMENT_H
#define DOCUMENT_H
#include <iostream>

using namespace std;
class Document {
public:
    Document() = default;
    Document(int id, double relevance, int rating);
    int id = 0;
    double relevance = 0.0;
    int rating = 0;

    friend ostream& operator<<(ostream &os, const Document& doc);
};

#endif // DOCUMENT_H
