#include "../headfile.h"

template<typename T>
class lock_free_stack {
private:
    struct count_node;
    std::atomic<count_node> head;
    struct node {
        std::shared_ptr<T> data;
        std::atomic<int> internal_count;
        count_node next;
        node(T const& data_) :data(std::make_shared<T>(data_)), internal_count(0) {}
    };
    struct count_node {
        int external_count;
        node* ptr;
    };
    void increase_head_count(count_node& old_counter) {
        count_node new_counter;
        do {
            new_counter = old_counter;
            ++new_counter.external_count;
        } while (!head.compare_exchange_strong(old_counter, new_counter,
                                               std::memory_order_acquire,
                                               std::memory_order_relaxed));
        old_counter.external_count = new_counter.external_count;
    }
public:
    void push(T const& data) {
        count_node new_node;
        new_node.ptr = new node(data);
        new_node.external_count = 1;
        new_node.ptr->next = head.load();
        while (!head.compare_exchange_weak(new_node.ptr->next, new_node,
                                           std::memory_order_release,
                                           std::memory_order_relaxed));
    }//分离引用计数的方式推送一个节点到无锁栈中
    std::shared_ptr<T> pop() {
        count_node old_head = head.load(std::memory_order_relaxed);
        for (;;) {
            increase_head_count(old_head);
            node* const ptr = old_head.ptr;
            if (!ptr) { return std::shared_ptr<T>(); }
            if (head.compare_exchange_strong(old_head, ptr->next, std::memory_order_relaxed)) {
                std::shared_ptr<T> res;
                res.swap(ptr->data);
                int const count_increase = old_head.external_count - 2;
                if (ptr->internal_count.fetch_add(count_increase,
                                                  std::memory_order_release) == -count_increase) { delete ptr; }
                return res;
            }
            else if (ptr->internal_count.fetch_sub(-1, std::memory_order_relaxed) == 1) {
                ptr->internal_count.load(std::memory_order_acquire);
                delete ptr;
            }
        }
        //无锁结构的复杂性主要在于内存的管理
        //需要先检查操作之间的依赖关系,而后再去确定适合这种需求关系的最小内存序
    }//分离引用计数从无锁栈中弹出一个节点
    ~lock_free_stack() {
        while (pop());
    }
};

template<typename T>
class lock_free_queue {
private:
    struct count_node_ptr;
    struct node_counter;
    struct node {
        std::atomic<T> data;
        std::atomic<node_counter> count;
        std::atomic<count_node_ptr> next;
        node() {
            node_counter new_count;
            new_count.internal_count = 0;
            new_count.external_count = 2;
            //新节点必定会被tail 和 上一个节点的next所指向
            count.store(new_count);
            next.ptr = nullptr;
            next.external_count = 0;
        }
        void release_ref() {
            node_counter old_counter = count.load(std::memory_order_relaxed);
            node_counter new_counter;
            do {
                new_counter = old_counter;
                --new_counter.internal_count;
            } while (!count.compare_exchange_strong(old_counter, new_counter,
                                                    std::memory_order_acquire,
                                                    std::memory_order_relaxed));
            if (new_counter.internal_count && !new_counter.external_count) { delete this; }
            //内部,外部计数全部为0 表示为最后一次使用 使用后可以删除
        }
    };
    struct count_node_ptr {
        int external_count;
        node* ptr;
    };
    struct node_counter {
        int internal_count : 30;
        int external_counters : 2;
        //这里是将计数器总大小设置为30bit 和 2bit
        //保证计数器大小总体为32bit 使其可以放入一个机器字中
    };
    std::atomic<count_node_ptr*> head;
    std::atomic<count_node_ptr*> tail;
    node* pop_head() {
        node* const old_head = head.load();
        if (old_head == tail.load()) { return nullptr; }
        head.store(old_head->next);
        return old_head;
    }
    static void increase_external_count(std::atomic<count_node_ptr>& counter,
                                        count_node_ptr& old_counter) {
        count_node_ptr new_counter;
        do {
            new_counter = old_counter;
            ++new_counter.external_count;
        } while (!counter.compare_exchange_strong(old_counter, new_counter,
                                                  std::memory_order_acquire,
                                                  std::memory_order_relaxed));
        old_counter.external_count = new_counter.external.count;
    }
    static void free_external_counter(count_node_ptr& old_node_ptr) {
        node* const ptr = old_node_ptr.ptr;
        int const count_increase = old_node_ptr.external_count - 2;
        node_counter old_counter = ptr->count.load(std::memory_order_relaxed);
        node_counter new_counter;
        do {
            new_counter = old_counter;
            --new_counter.external_counters;
            new_counter.internal_count += count_increase;
        } while (!ptr->count.compare_exchange_strong(old_counter, new_counter,
                                                     std::memory_order_acquire,
                                                     std::memory_order_relaxed));
        //对计数结构体中的计数器进行更新
        if (!new_counter.internal_count && !new_counter.external_counters) { delete ptr; }
        //内外计数值都为0,没有更多的节点可以被引用,所以可以安全的删除节点
    }
    void set_new_tail(count_node_ptr& old_tail, count_node_ptr const& new_tail) {
        node* const current_tail_ptr = old_tail.ptr;
        while (!tail.compare_exchange_weak(old_tail, new_tail) &&
               old_tail.ptr == current_tail_ptr) { ; }
        if (old_tail.ptr == current_tail_ptr) { free_external_counter(old_tail); }
        //当新旧ptr相同时,循环退出,代表对tail的设置已经完成,所以需要释放旧外部计数器
        else { current_tail_ptr->release_ref(); }
        //当ptr值不一样时另一线程可能已经将计数器释放了,所以只需要对该线程持有的单次引用进行释放即可
    }
public:
    lock_free_queue() :head(new node), tail(head.load()) {}
    lock_free_queue(const lock_free_queue& other) = delete;
    lock_free_queue& operator=(const lock_free_queue& other) = delete;
    ~lock_free_queue() {
        while (node* const old_head = head.load()) {
            head.store(old_head->next);
            delete old_head;
        }
    }
    void push(T value) {
        std::unique_ptr<T> new_data(new T(value));
        count_node_ptr new_next;
        new_next.ptr = new node;
        new_next.external_count = 1;
        count_node_ptr old_tail = tail.load();
        for (;;) {
            increase_external_count(tail, old_tail);
            T* old_data = nullptr;
            if (old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get())) {
                count_node_ptr old_next = { 0 };
                if (!old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
                //当交换失败就能知道另有线程对next指针进行设置,所以就可以删除一开始分配的那个新节点
                    delete new_next.ptr;
                    new_next = old_next;
                }
                set_new_tail(old_tail, new_next);
                new_data.release();
                break;
            }
            else {
                count_node_ptr old_next = { 0 };
                if (old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
                //尝试更新next指针，让其指向该线程分配出来的新节点
                //指针更新成功时，就可以将这个新节点作为新的tail节点
                    old_next = new_next;
                    new_next.ptr = new node;
                //需要分配另一个新节点用来管理队列中新推送的数据项
                }
                set_new_tail(old_tail, old_next);
                //在进入循环之前,可以通过调用set_new_tail来设置tail节点
            }
        }
        //新节点在push()中被分配,而在pop()中被销毁
        //高效的内存分配器也很重要(其他途径了解)
    }
    std::unique_ptr<T> pop() {
        count_node_ptr old_head = head.load(std::memory_order_relaxed);
        for (;;) {
            increase_external_count(head, old_head);
            node* const ptr = old_head.ptr;
            if (ptr == tail.load().ptr) {
                return std::unique_ptr<T>();
            }
            count_node_ptr next = ptr->next.load();
            if (head.compare_exchange_strong(old_head, next)) {
                T* const res = ptr->data.exchange(nullptr);
                free_external_count(old_head);
                return std::unique_ptr<T>(res);
            }
            ptr->release_ref();
        }
    }
};
//设计无锁数据结构是一项很困难的任务,并且很容易犯错
//不过这样的数据结构在某些重要情况下可对其性能会有加强
//无锁数据结构的实现过程中,需要小心使用原子操作的内存序
//保证无数据竞争,以及让每个线程看到一个数据结构实例
//在无锁结构中对内存的管理是越来越难

int main() {
    lock_free_stack<int> stk1;
}