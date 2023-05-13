#include "document.h"
Document::Document(int id, double relevance, int rating)
    : id(id)
    , relevance(relevance)
    , rating(rating) {
}

ostream& operator<<(ostream &os, const Document& doc) {
    return os << "{ "s
              << "document_id = "s << doc.id << ", "s
              << "relevance = "s << doc.relevance << ", "s
              << "rating = "s << doc.rating
              << " }"s;
}
