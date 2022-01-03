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
    //互斥量mtx可保证线程安全,对每个成员函数进行加锁保护,保证在同一时间内,只有一个线程可以访问到数据
    //在empty()和pop()成员函数之间会存在竞争,不过代码会在pop()函数上锁时,显式的查询栈是否为空
    //构造与析构函数不是线程安全的
    //所以要保证在栈对象完成构建前,其他线程无法对其进行访问,并且要保证在栈对象销毁后,所有线程都要停止对其进行访问。,
    //不仅要保证多线程使用安全,单线程下也要保证效率
    //序列化线程会隐式限制性能,且线程在等待锁或是等待添加数据会无意义的检查empty()或pop()导致资源浪费
};