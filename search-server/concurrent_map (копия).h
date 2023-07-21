#pragma once
#include <map>
#include <mutex>
#include <vector>
template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");
    struct Bucket{
        std::mutex mtx;
        std::map<Key, Value> mp;
    };

    struct Access {
        std::lock_guard<std::mutex> grd;
        Value& ref_to_value;
        Access(const Key &key, Bucket& bucket):
            grd(bucket.mtx), ref_to_value(bucket.mp[key]){
        }
        Access(const Access &) = delete;
    };

    explicit ConcurrentMap(size_t bucket_count = 1): vec_bucket_(bucket_count){
    }

    Access operator[](const Key& key){
        auto &bucket = vec_bucket_[static_cast<uint64_t>(key) % vec_bucket_.size()];
        return {key, bucket};
    }

    std::map<Key, Value> BuildOrdinaryMap(){
        std::map<Key, Value> result;
        for (auto &x: vec_bucket_){
            result.insert(x.mp.begin(), x.mp.end());
        }
        return result;
    }

    void Erase(int document_id){
        Bucket &x = vec_bucket_.at(static_cast<uint64_t>(document_id) % vec_bucket_.size());
        std::lock_guard guard(x.mtx);
        x.mp.erase(document_id);
    }

private:
    std::vector<Bucket> vec_bucket_;
};
