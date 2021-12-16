#include "headfile.h"
//设计并发数据结构时需要考量两方面:一是确保访问安全,二是真正并发访问



//使用锁的线程安全的栈
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

//使用锁和条件变量的线程安全的队列
template<typename T>
class thread_safe_queue2 {
private:
    mutable std::mutex mtx;
    std::queue<std::shared_ptr<T>> data;
    //使用shared_ptr对队列的性能有很大的提升,其减少了互斥量持有的时间
    //允许其他线程在分配内存(时间相对较久)的同时,对队列进行其他的操作
    std::condition_variable cond;
public:
    thread_safe_queue2() {}
    void push(T value_) {
        std::shared_ptr<T> value(std::make_shared<T>(std::move(value_)));
        std::lock_guard<std::mutex> lk(mtx);
        data.push(value);
        cond.notify_one();
    }
    void wait_pop(T& value) {
        std::unique_lock<std::mutex> lk(mtx);
        cond.wait(lk, [this] {return !data.empty();});
        value = std::move(*data.front());
        data.pop();
        //wait_pop中使用cond.wait(),比持续调用empty要好很多
    }
    std::shared_ptr<T> wait_pop() {
        std::unique_lock<std::mutex> lk(mtx);
        cond.wait(lk, [this] { return !data.front();});
        std::shared_ptr<T> res(data.front());
        data.pop();
        return res;
    }
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(mtx);
        if (data.empty()) { return false; }
        value = std::move(*data.front());
        data.pop();
        return true;
    }
    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(mtx);
        if (data.empty()) { return std::shared_ptr<T>(); }
        std::shared_ptr<T> res = data.front();
        data.pop();
        return res;
    }
    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx);
        return data.empty();
    }
    //一个互斥量来保护整个数据结构会导致这个队列对并发的支持被限制
};

//使用细粒度锁和条件变量的线程安全的队列
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
        std::unique_ptr<node> old_head = std::move(head);  
        head = std::move(old_head->next);
        return old_head;
        
    }//try_pop的代码复用
    std::unique_lock<std::mutex> wait_data() {
        std::unique_lock<std::mutex> head_lk(head_mtx);
        cond.wait(head_lk, [&] { return head.get() != get_tail(); });
        return std::move(head_lk);
        
    }//等待条件变量,同时会返回锁
    std::unique_ptr<node> wait_pop_head() {
        std::unique_lock<std::mutex> head_lk(wait_data());
        return pop_head();
        
    }//删除头节点
    std::unique_ptr<node> wait_pop_head(T& value) {
        std::unique_ptr<std::mutex> head_lk(wait_data());
        value = std::move(*head->data);
        return pop_head();
        
    }//删除头节点
    //提供简单的操作,减低复杂度
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
    thread_safe_queue() :head(new node), tail(head.get()) {};
    thread_safe_queue(const thread_safe_queue& other) = delete;
    thread_safe_queue& operator=(const thread_safe_queue& other) = delete;
    std::shared_ptr<T> try_pop() {
        std::unique_ptr<node> old_head = try_pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
    bool try_pop(T& value) {
        std::unique_ptr<node> const old_head = try_pop_head(value);
        return old_head;
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
        //仅修改了被tail_mtx的数据
        //新的尾节点是一个空节点,并且其data和next都为旧的尾节点
    }
    bool empty() {
        std::lock_guard<std::mutex> head_lk(head_mtx);
        return (head.get() == get_tail());
    }
};
//一个无界队列,指线程可以持续向队列中添加数据项,即使没有元素被删除
//有界队列是在创建的时候最大长度就已经是固定的了
//当有界队列满载时尝试在向其添加元素的操作将会失败或者阻塞,直到有元素从队列中弹出


int main() {
    thread_safe_stack<int> S1;
    thread_safe_queue2<int> Q1;
    thread_safe_queue<int> Q2;
}