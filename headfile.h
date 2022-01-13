#include <thread>
#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <numeric>      //accumulate
#include <functional>   //mem_fn
#include <utility>      //forward
#include <stdarg.h>     //... var
#include <condition_variable> 
#include <future>       //async future
#include <chrono>
#include <list>
#include <stack>
#include <queue>
#include <map>
#include <mutex>        //mutex,lock_guard,lock,scoped_lock
#include <shared_mutex> //only in c++14,c++17 
#include <exception>    
#include <memory>
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <execution>    //执行策略


using Ulong = unsigned long;
using Us = std::chrono::microseconds;
using Ms = std::chrono::milliseconds;
using Sec = std::chrono::seconds;
using SysClock = std::chrono::system_clock;
using HighClock = std::chrono::high_resolution_clock;
using SteadyClock = std::chrono::steady_clock;
template<typename Rep, typename Period>
using Duration = std::chrono::duration<Rep, Period>;
template<typename Clock, typename Duration>
using TimePoint = std::chrono::time_point<Clock, Duration>;


int plus(int a, int b) { return a + b; }
double square(double x) {
    if (x < 0) {
        throw std::out_of_range("x < 0");
    }
    return sqrt(x);
}
int ret_num() { return 42; }

void do_something() { std::cout << "print something.\n"; }
void do_something_else() { std::cout << "print another.\n"; }
void print_something(int i) { std::cout << "print" << i << "\n"; }
void thread_do_something(int& i, std::string& str) { std::cout << "num:" << i << " string:" << str << "\n"; }
void do_something_in_current_thread() { std::cout << "thread current do someting...\n"; }

class join_threads {
private:
    std::vector<std::thread>& threads;
public:
    explicit join_threads(std::vector<std::thread>& threads_) :threads(threads_) {}
    ~join_threads() {
        for (int i = 0; i < threads.size(); i++) {
            if (threads[i].joinable()) { threads[i].join(); }
        }
    }
};
//无论线程如何离开这段代码,所有线程都可以被汇入

template <typename Iterator, typename T>
struct accumulate_block {
    void operator()(Iterator first, Iterator last, T& result) {
        result = std::accumulate(first, last, result);
    }
};

struct empty_stack : std::exception {
    const char* what() const throw() { return "empty stack!"; }
};
template<typename T>
class thread_safe_stack {
private:
    std::stack<T> data;
    mutable std::mutex mtx;
public:
    thread_safe_stack() {};
    thread_safe_stack(const thread_safe_stack& other) {
        std::lock_guard<std::mutex> lk(other.mtx);
        data = other.data;
    }
    thread_safe_stack& operator=(const thread_safe_stack&) = delete;
    void push(T value) {
        std::lock_guard<std::mutex> lk(mtx);
        data.push(std::move(value));
    }
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> lk(mtx);
        std::shared_ptr<T> const res(std::make_shared<T>(std::move(data.top())));
        data.pop();
        return res;
    }
    void pop(T& value) {
        std::lock_guard<std::mutex> lk(mtx);
        if (data.empty()) throw empty_stack();
        value = std::move(data.top());
        data.pop();
    }
    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx);
        return data.empty();
    }
    //chapter5
};

template <typename T>
class thread_safe_queue {
private:
    struct node {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    std::unique_ptr<node> head;
    node* tail;
    std::mutex head_mtx;
    std::mutex tail_mtx;
    std::condition_variable cond;
    node* get_tail() {
        std::lock_guard<std::mutex> tail_lk(tail_mtx);
        return tail;
    }
    std::unique_ptr<node> pop_head() {
        std::lock_guard<std::mutex> head_lock(head_mtx);
        if (head.get() == get_tail()) {
            return nullptr;
        }
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }
    std::unique_lock<std::mutex> wait_data() {
        std::unique_lock<std::mutex> head_lk(head_mtx);
        cond.wait(head_lk, [&] { return head.get() != get_tail(); });
        return std::move(head_lk);
    }
    std::unique_ptr<node> wait_pop_head() {
        std::unique_lock<std::mutex> head_lk(wait_data());
        return pop_head();
    }
    std::unique_ptr<node> wait_pop_head(T& value) {
        std::unique_ptr<std::mutex> head_lk(wait_data());
        value = std::move(*head->data);
        return pop_head();

    }
    std::unique_ptr<node> try_pop_head() {
        std::lock_guard<std::mutex> head_lk(head_mtx);
        if (head.get() == get_tail()) {
            return std::unique_ptr<node>();
        }
        return pop_head();
    }
    std::unique_ptr<node> try_pop_head(T& value) {
        std::lock_guard<std::mutex> head_lk(head_mtx);
        if (head.get() == get_tail()) {
            return std::unique_ptr<node>();
        }
        value = std::move(*head->data);
        return pop_head();
    }
public:
    thread_safe_queue() :head(new node), tail(head.get()) {}
    thread_safe_queue(const thread_safe_queue& other) = delete;
    thread_safe_queue& operator=(const thread_safe_queue& other) = delete;
    std::shared_ptr<T> try_pop() {
        std::unique_ptr<node> old_head = try_pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
    bool try_pop(T& value) {
        std::unique_ptr<node> old_head = try_pop_head(value);
        return old_head ? true : false;
    }
    std::shared_ptr<T> wait_pop() {
        std::unique_ptr<node> const old_head = wait_pop_head();
        return old_head->data;
    }
    void wait_pop(T& value) {
        std::unique_ptr<node> const old_head = wait_pop_head(value);
    }
    void push(T value) {
        std::shared_ptr<T> new_data(std::make_shared<T>(std::move(value)));
        std::unique_ptr<node> tmp(new node);
        {
            std::lock_guard<std::mutex> tail_lk(tail_mtx);
            tail->data = new_data;
            node* const new_tail = tmp.get();
            tail->next = std::move(tmp);
            tail = new_tail;
        }
        cond.notify_one();
    }
    bool empty() {
        std::lock_guard<std::mutex> head_lk(head_mtx);
        return (head.get() == get_tail());
    }
    //chapter5
};