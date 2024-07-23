#include <algorithm>
#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

inline std::chrono::microseconds::rep getCurrentTimestamp() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Adapted from C++ Concurrency in Action Listing 6.11. A thread-safe lookup table
// https://livebook.manning.com/book/c-plus-plus-concurrency-in-action-second-edition/chapter-6/135
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class TSMap
{
private:
    class Bucket
    {
    private:
        typedef std::pair<Key, Value*> bucket_value;
        typedef std::list<bucket_value> bucket_data;
        typedef typename bucket_data::iterator bucket_iterator;

        bucket_data data;
        mutable std::mutex m;

    public:
        Bucket() : data{}, m{} {};

        struct FindInfo
        {
            Value* value;
            intmax_t timestamp = getCurrentTimestamp();

            FindInfo(Value* value) : value{value} {};
        };
        FindInfo find(const Key& key)
        {
            std::unique_lock<std::mutex> lock{m};
            bucket_iterator value = std::find_if(
                data.begin(),
                data.end(),
                [&](const bucket_value& value) { return value.first == key; });
            if (value != data.end())
            {
                return FindInfo{value->second};
            }
            else
            {
                return FindInfo{nullptr};
            }
        };

        void emplace_back(const Key& key, Value* value)
        {
            std::unique_lock<std::mutex> lock{m};
            data.emplace_back(key, value);
        };

        Value* get(const Key& key)
        {
            std::unique_lock<std::mutex> lock{m};
            bucket_iterator value = std::find_if(
                data.begin(),
                data.end(),
                [&](const bucket_value& value) { return value.first == key; });
            if (value != data.end())
            {
                // Get
                return value->second;
            }
            else
            {
                // Add
                data.emplace_back(key, new Value{});
                // Get
                return data.back().second;
            }
        };

        void erase(const Key& key)
        {
            std::unique_lock<std::mutex> lock{m};
            bucket_iterator value = std::find_if(
                data.begin(),
                data.end(),
                [&](const bucket_value& value) { return value.first == key; });
            if (value != data.end())
            {
                data.erase(value);
            }
        };
    };
    
    std::vector<std::unique_ptr<Bucket>> buckets;
    Hash hasher;

    Bucket& get_bucket(const Key& key) const
    {
        const std::size_t bucket_index = hasher(key) % buckets.size();
        return *buckets[bucket_index];
    };

public:
    // 19 is an arbitrary prime number.
    // Hash tables work best with a prime number of buckets.
    TSMap(unsigned num_buckets = 19, const Hash& hasher_ = Hash())
        : buckets{num_buckets}, hasher{hasher_}
    {
        for (unsigned i = 0; i < num_buckets; ++i)
        {
            buckets[i].reset(new Bucket{});
        }
    };
    // TODO: Destructor
    TSMap(const TSMap& other) = delete;
    TSMap& operator=(const TSMap& other) = delete;

    // Return pointer to existing value if found,
    // otherwise return nullptr.
    typename Bucket::FindInfo find(const Key &key)
    {
        // Concurrency is enabled via bucket-ization of key-value pairs.

        // get_bucket() can be called without any locking
        // as buckets is read-only since initialization.
        return get_bucket(key).find(key);
    };

    // Add key-value pair without checking for its existence.
    void emplace_back(const Key& key, Value* value)
    {
        return get_bucket(key).emplace_back(key, value);
    };

    // Return pointer to existing value if found,
    // otherwise add key-value pair and
    // return pointer to defaultly constructed value.
    Value* get(const Key& key)
    {
        return get_bucket(key).get(key);
    };

    // Delete key-value pair if exists, otherwise do nothing.
    void erase(const Key& key)
    {
        return get_bucket(key).erase(key);
    };
};
