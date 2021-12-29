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
        } while (!head.compare_exchange_strong(old_counter, new_counter));
    }
public:
    void push(T const& data) {
        count_node new_node;
        new_node.ptr = new node(data);
        new_node.external_count = 1;
        new_node.ptr->next = head.load();
        while (!head.compare_exchange_weak(new_node.ptr->next, new_node));
    }//分离引用计数的方式推送一个节点到无锁栈中
    std::shared_ptr<T> pop() {
        count_node old_head = head.load();
        for (;;) {
            increase_head_count(old_head);
            node* const ptr = old_head.ptr;
            if (!ptr) { return std::shared_ptr<T>(); }
            if (head.compare_exchange_strong(old_head, ptr->next)) {
                std::shared_ptr<T> res;
                res.swap(ptr->data);
                int const count_increase = old_head.external_count - 2;
                if (ptr->internal_count.fetch_add(count_increase) == -count_increase) { delete ptr; }
                return res;
            }
            else if (ptr->internal_count.fetch_sub(1) == 1) { delete ptr; }
        }
    }//分离引用计数从无锁栈中弹出一个节点
    ~lock_free_stack() {
        while (pop());
    }
};

int main() {
    lock_free_stack<int> stk1;
}