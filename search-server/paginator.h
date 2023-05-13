#pragma once
#ifndef PAGINATOR_H
#define PAGINATOR_H
#include <iostream>
#include <vector>

using namespace std;

template <typename Iterator>
class IteratorRange{
    pair<Iterator, Iterator> page;

public:
    Iterator begin() const {
        return page.first;
    }
    Iterator end() const {
        return page.second;
    }
    size_t size() const{
        return distance(page.begin, page.end);
    }
    IteratorRange(Iterator begin, Iterator end) : page{begin, end}{
    }
};

template <typename Iterator>
ostream& operator<<(ostream &os, const IteratorRange<Iterator>& rng) {
    for (auto i = rng.begin(); i != rng.end(); ++i){
        os << *i ;
    }
    return os;
}

template <typename Iterator>
class Paginator{
    vector<IteratorRange<Iterator>> pages;

public:
    auto begin() const{
        return pages.begin();
    }
    auto end() const{
        return pages.end();
    }
    Paginator(Iterator begin, Iterator end, size_t page_size){
        size_t dist = distance(begin, end);
        size_t size = dist / page_size;
        while(size){
            Iterator new_end = begin;
            advance(new_end, page_size);
            pages.push_back(IteratorRange(begin, new_end));
            advance(begin, page_size);
            --size;
        }
        auto ost = dist % page_size;
        if (ost > 0) {
            Iterator new_end = begin;
            advance(new_end, ost);
            pages.push_back(IteratorRange(begin, new_end));
        }
    }
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

#endif // PAGINATOR_H
