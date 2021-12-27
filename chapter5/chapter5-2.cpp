#include "../headfile.h"

//线程安全查询表 map

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class thread_safe_table {
private:
    using bucket_value = std::pair<Key, Value>;
    using bucket_data = std::list<bucket_value>;
    using bucket_iterator = typename bucket_data::iterator;
    class bucket_type {
    private:
        bucket_data data;
        mutable std::shared_mutex mtx;
        //这里的锁只在共享所有权和获取唯一读写权时上锁使用
        bucket_iterator find_entry(Key const& key) const {
            return std::find_if(data.begin(), data.end(),
                                [&](bucket_value const& item) { return item.first == key; });
        }//确认数据是否在桶中
    public:
        Value value(Key const& key, Value const& default_value) const {
            std::shared_lock<std::shared_mutex> lk(mtx);
            bucket_iterator const found_entry = find_entry(key);
            return (found_entry == data.end() ? default_value : found_entry->second);
        }
        void update_map(Key const& key, Value const& value) {
            std::unique_lock<std::shared_mutex> lk(mtx);
            bucket_iterator const found_entry = find_entry(key);
            if (found_entry == data.end()) {
                data.push_back(bucket_value(key, value));
            }
            else { found_entry->second = value; }
        }
        void remove_map(Key const& key) {
            std::unique_lock<std::shared_mutex> lk(mtx);
            bucket_iterator const found_entry = find_entry(key);
            if (found_entry != data.end()) {
                data.erase(found_entry);
            }
        }
    };
    std::vector<std::unique_ptr<bucket_type>> buckets;
    Hash hashes;
    bucket_type& get_bucket(Key const& key) const {
        std::size_t const bucket_index = hashes(key) % buckets.size();
        return *buckets[bucket_index];
    }
public:
    thread_safe_table(int num_buckets = 19, Hash const& hashes_ = Hash()) :
        buckets(num_buckets), hashes(hashes_) {
        for (int i = 0;i < num_buckets;i++) {
            buckets[i].reset(new bucket_type);
        }
    }//指定默认数量为19(哈希表在质数个桶时效率最高)
    thread_safe_table(thread_safe_table const& other) = delete;
    thread_safe_table& operator=(thread_safe_table const& other) = delete;
    Value value_for(Key const& key, Value const& default_value = Value()) const {
        return get_bucket(key).value_for(key, default_value);
        //数量固定 因此可以无锁调用
    }
    void update_map(Key const& key, Value const& value) {
        get_bucket(key).update_map(key, value);
    }
    void remove_map(Key const& key) {
        get_bucket(key).remove_map(key);
    }
    std::map<Key, Value> get_map() const {
        std::vector<std::unique_lock<std::shared_mutex>> lks;
        for (int i = 0;i < buckets.size();i++) {
            lks.push_back(std::unique_lock<std::shared_mutex>(buckets[i].mtx));
        }
        std::map<Key, Value> res;
        for (int i = 0;i < buckets.size();i++) {
            for (bucket_iterator it = buckets[i].data.begin();
                 it != buckets[i].data.end();it++) {
                res.insert(*it);
            }
        }
        return res;
        //有可无(nice-to-have)的特性,会将选择当前状态的快照 例如一个std::map<>
        //这要求锁住整个容器,保证拷贝副本的状态是可以索引的,这将锁住所有的桶
        //因为对于查询表的普通的操作,需要在同一时间获取桶上的锁,而这个操作将要求查询表将所有桶都锁住
        //因此只要每次以相同的顺序进行上锁(例如，递增桶的索引值),就不会产生死锁
    }//获取整个thread_safe_table
    //这个查询表作为一个整体,通过单独的操作,对每一个桶进行锁定
    //并且通过使用std::shared_mutex允许读者线程对每一个桶并发访问,增大了并发访问的能力
};

template<typename T>
class thread_safe_list {
private:
    struct node {
        std::mutex mtx;
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
        node() :next() {}
        node(T const& value) :data(std::make_shared<T>(value)) {}
    };
    node head;
public:
    thread_safe_list() {}
    ~thread_safe_list() { remove_if([](node const&) { return true; }); }
    thread_safe_list(thread_safe_list const& other) = delete;
    thread_safe_list& operator=(thread_safe_list const& other) = delete;
    void push_front(T const& value) {
        std::unique_ptr<node> new_node(new node(value));
        std::lock_guard<std::mutex> lk(head.mtx);
        new_node->next = std::move(head.next);
        head.next = std::move(new_node);
    }
    template<typename Func>
    void for_each(Func func) {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.mtx);
        while (node* const next = current->next.get()) {
            std::unique_lock<std::mutex> next_lk(next->mtx);
            lk.unlock();
            func(*next->data);
            current = next;
            lk = std::move(next_lk);
        }
    }
    template<typename Predcate>
    std::shared_ptr<T> find_first_if(Predcate predcate) {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.mtx);
        while (node* const next = current->next.get()) {
            std::unique_lock<std::mutex> next_lk(next->mtx);
            lk.unlock();
            if (predcate(*next->data)) {
                return next->data;
            }
            current = next;
            lk = std::move(next_lk);
        }
        return std::shared_ptr<T>();
    }
    template<typename Predcate>
    void remove_if(Predcate predcate) {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.mtx);
        while (node* const next = current->next.get()) {
            std::unique_lock<std::mutex> next_lk(next->mtx);
            if (predcate(*next->data)) {
                std::unique_lock<std::mutex> next_lk(next->mtx);
                if (predcate(*next->data)) {
                    std::unique_ptr<node> old_next = std::move(current->next);
                    current->next = std::move(next->next);
                    next_lk.unlock();
                }
                else {
                    lk.unlock();
                    current = next;
                    lk = std::move(next_lk);
                }
            }
        }
    }
};

int main() {
    thread_safe_table<int, int> T1;
    thread_safe_list<int> L1;
}