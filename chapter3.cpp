#include "headfile.h"

#include <condition_variable> 

using Ms = std::chrono::microseconds;
using Sec = std::chrono::seconds;

bool flag1;
std::mutex mtx1;
void wait_for_sleep() {
    std::unique_lock<std::mutex> lk(mtx1);
    while (!flag1) {
        lk.unlock();
        std::this_thread::sleep_for(Ms(100));
        //休眠时线程不会与其他线程竞争CPU
        //利用休眠来等待事件或条件完成会无法确定合适的时间长度
        lk.lock();
    }
}

std::mutex mtx2;
std::queue<int> data_queue;
std::condition_variable data_cond;
bool done_flag = false;
std::condition_variable preocess_done;
void data_preparation_thread() {
    int data = 10;
    while (data < 15) {
        std::lock_guard<std::mutex> lk(mtx2);
        data_queue.push(data);
        data_cond.notify_all();
        data++;
        std::cout << "processing...\n";
        std::this_thread::sleep_for(Sec(1));
    }
}
void data_processing_thread() {
    while (true) {
        std::unique_lock<std::mutex> lk(mtx2);
        data_cond.wait(lk, [] {return !data_queue.empty();});
        int data = data_queue.front();
        data_queue.pop();
        std::cout << "Get data:" << data << "\n";
        if (data_queue.empty()) {
            std::cout << "data queue is empty\n";
            done_flag = true;
            preocess_done.notify_one();
            break;
        } 
    }
}
void func1() {
    std::thread T1(data_preparation_thread);
    T1.detach();
    std::thread T2(data_processing_thread);
    T2.detach();
    std::mutex done_mtx;
    std::unique_lock<std::mutex> lk(done_mtx);
    preocess_done.wait(lk, [] { return done_flag; });
    //condition_variable需要搭配unique_lock和一个函数进行使用
    //当wait被调用时,通过unique_lock来锁住当前线程,使其一直阻塞
    //直到另一个线程在相同的condition_variable对象上调用notify来唤醒当前线程
    std::cout << "All done, main thread quit.\n";
}
//利用条件变量来控制线程的休眠
//使之在有数据可处理时处理,没有时则休眠,不会一直占用资源
//线程被唤醒后需要检查条件是否满足 
//无论是notify_one或notify_all都是类似于发出脉冲信号
//如果对wait的调用发生在notify之后是不会被唤醒的,所以接收者在使用wait等待之前也需要检查条件是否满足
//std::condition_variable对象通常使用std::unique_lock<std::mutex>来等待
//如果需要使用另外的lockable类型,可以使用std::condition_variable_any类

/*
template <class T, class Container = std::deque<T>>
class queue {
public:
    explicit queue(const Container&);
    explicit queue(Container && = Container());
    template <class Alloc> explicit queue(const Alloc&);
    template <class Alloc> queue(const Container&, const Alloc&);
    template <class Alloc> queue(Container&&, const Alloc&);
    template <class Alloc> queue(queue&&, const Alloc&);
    void swap(queue& q);
    bool empty() const;
    Ulong size() const;
    T& front();
    const T& front() const;
    T& back();
    const T& back() const;
    void push(const T& x);
    void push(T&& x);
    void pop();
    template <class... Args> void emplace(Args&&... args);
    //省略了赋值以及交换操作
};
*/

template<typename T>
class thread_safe_queue {
private:
    std::mutex mtx;
    std::queue<T> data_queue;
    std::condition_variable data_cond;
public:
    thread_safe_queue() {};
    thread_safe_queue(const thread_safe_queue& other) {
        std::lock_guard<std::mutex> lk(other.mtx);
        data_queue = other.data_queue;
    }
    thread_safe_queue& operator=(const thread_safe_queue&) = delete;
    void push(T value) {
        std::unique_lock<std::mutex> lk(mtx);
        data_queue.push(value);
        lk.unlock();
        data_cond.notify_one();
        //unlock在notify后有时会导致无法成功解锁
    }
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(mtx);
        if (data_queue.empty()) { return false; }
        value = data_queue.front();
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(mtx);
        if (data_queue.empty()) { return std::shared_ptr<T>(); }
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }
    void wait_pop(T& value) {
        std::unique_lock<std::mutex> lk(mtx);
        data_cond.wait(lk, [this] { return !data_queue.empty(); });
        //wait 传入锁是为了保护外部的条件函数到睡眠状态的间隙,避免被其他线程打断从而丢失唤醒机会(虚假唤醒)
        //虚假唤醒是指在等待wait条件变量时,会在没有被其他线程通知的情况下提前结束等待
        //虚假唤醒是偶发的,所以需要在条件变量等待结束时判断是否满足了执行条件
        value = data_queue.front();
        data_queue.pop();
    }
    std::shared_ptr<T> wait_pop() {
        std::unique_lock<std::mutex> lk(mtx);
        data_cond.wait(lk, [this] { return !data_queue.empty(); });
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }
    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx);
        return data_queue.empty();
    }
    //保证线程安全,禁止了简单赋值
};

int main() {
    func1();
}