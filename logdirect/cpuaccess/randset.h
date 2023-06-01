#include <cstdio>
#include<cstdlib>
#include <random>
#include <unordered_map>

template<typename T>
class RandSet {
public:
    RandSet() : num(0), dist(0, INT32_MAX) {
        gen.seed(time(NULL));
    }

    RandSet(const RandSet &) = delete;
    RandSet(RandSet &&) = delete;
    RandSet & operator = (const RandSet &) = delete;
    RandSet & operator = (RandSet &&) = delete;

    inline bool insert(const T &t) {
        auto it = to_index.find(t);
        if(it == to_index.end()) {
            to_index.insert({t, num});
            to_key.insert({num, t});
            num += 1;

            return true;
        } else {
            return false;
        }
    }

    inline bool remove(const T &t) {
        auto it = to_index.find(t);
        if(it == to_index.end()) {
            return false;
        } else {
            if(num > 1 && it->second != num - 1) { 
                // replace the last key with the deleted key
                int cur_idx = it->second;
                T last_one = to_key[num - 1];

                // update the connection with (cur_idx, lastone)
                to_key[cur_idx] = last_one;
                to_index[last_one] = cur_idx;

                // delete the mapping of the last key
                to_key.erase(num - 1);
                // delete the mapping of the deleted key
                to_index.erase(t);
            } else {
                // delete the mapping of the deleted key
                to_key.erase(it->second);
                to_index.erase(t);
            }

            num -= 1;

            return true;
        }
    }

    inline T get() {
        int randnum = dist(gen) % num;
        auto it = to_key.find(randnum);
        return it->second;
    }

    inline int size() const {
        return num;
    }

private:
    std::default_random_engine gen;
    std::uniform_int_distribution<int> dist;
    int num;

    std::unordered_map<T, int> to_key;
    std::unordered_map<int, T> to_index;
};