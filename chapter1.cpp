#include "headfile.h"
//基础操作和STL

class background_task {
public:
    void operator()() const {
        do_something();
        do_something_else();
    }
};
class thread_guard {
    std::thread& t;
public:
    explicit thread_guard(std::thread& t_) :t(t_) {}
    ~thread_guard() {
        if (t.joinable()) {
            std::cout << "wait thread...\n";
            t.join();
        }
    }
    thread_guard(thread_guard const&) = delete;
    thread_guard operator=(thread_guard const&) = delete;
};

/*class scoped_thread {
    std::thread t;
public:
    explicit scoped_thread(std::thread t_) :
        t(std::move(t_)) {
        if (std::move(t_)) {
            throw std::logic_error("No thread");
        }
    }
    ~scoped_thread() { t.join(); }
    scoped_thread(scoped_thread const&) = delete;
    scoped_thread& operator=(scoped_thread const&) = delete;
};*/
struct func {
    int& i;
    func(int& i_) :i(i_) {}
    void operator()() {
        for (int j = 0; j != 3; j++) {
            sleep(1);
            print_something(j);
        }
    }
    ~func() { std::cout << "func end\n"; }
    //会被线程调用两次？？
};

void oops1() {
    int local_state = 0;
    func errorfunc(local_state);
    std::thread T2(errorfunc);
    T2.detach();
}
void correct1() {
    //...
}

void func1() {
    int local_state = 0;
    func my_func(local_state);
    std::thread t(my_func);
    thread_guard g(t);
    do_something_in_current_thread();
}

void update_data(int num, int& data) {
    std::cout << "num:" << num << ", Now equal:" << data << "\n";
}
void oops2() {
    int num = 10;
    int data = 0;
    //std::thread t(update_data, num, data);
    //传递引用不能直接传递右值 需要改为引用形式
    std::thread t(update_data, num, std::ref(data));
    t.join();
}

class X {
public:
    void do_lengthy_work() {
        for (int j = 0; j != 3; j++) {
            sleep(1);
            print_something(j);
        }
    }
};
void func2() {
    X my_x;
    std::thread t(&X::do_lengthy_work, &my_x);
    t.join();
    //新线程将my_x.do_lengthy_work()作为线程函数
    //my_x的地址作为指针对象提供给函数
}

void func3() {
    std::thread t1(do_something);
    std::thread t2 = std::move(t1);
    std::thread t3;
    t1 = std::thread(do_something_else);
    t3 = std::move(t2);
    //t1 = std::move(t3);
    t1.join();
    //t2.join();
    t3.join();
    //t1 一开始有任务执行
    //t2 显式转移了线程t1
    //t3 初始化无任务
    //t1 被转移后无任务 可以被重新指派
    //t3 初始化后无任务 可以转移其他任务至此线程
    //t1=move(t3) 此时的t1有任务 无法被指派其他任务 
    //执行会报错 系统抛出异常terminate 为了保证线程对象的完整执行
}

class scoped_thread {
    std::thread t;
public:
    explicit scoped_thread(std::thread t_) :
        t(std::move(t_)) {
        if (!t.joinable()) {
            throw std::logic_error("Empty thread");
        }
    }
    ~scoped_thread() { t.join(); }
    scoped_thread(scoped_thread const&) = delete;
    scoped_thread operator=(scoped_thread const&) = delete;
};
void func4() {
    int local_state;
    scoped_thread t{ std::thread(func(local_state)) };
    do_something_in_current_thread();
}

class joining_thread {
    std::thread t;
public:
    joining_thread() noexcept = default;
    template<typename Call, typename ... Args>
    explicit joining_thread(Call&& func, Args&& ... args) :
        t(std::forward<Call>(func), std::forward<Args>(args)...) {}
    explicit joining_thread(std::thread t_) noexcept :
        t(std::move(t_)) {};
    joining_thread(joining_thread&& other) noexcept : t(std::move(other.t)) {};
    joining_thread& operator=(joining_thread&& other) noexcept {
        if (joinable()) { join(); }
        t = std::move(other.t);
        return *this;
    }
    void swap(joining_thread& other) noexcept { t.swap(other.t); }
    std::thread::id get_id() const noexcept { return t.get_id(); }
    bool joinable() const noexcept { return t.joinable(); }
    void join() { t.join(); }
    void detach() { t.detach(); }
    std::thread& as_thread() noexcept { return t; }
    const std::thread& as_thread() const noexcept { return t; }
    ~joining_thread() noexcept { if (joinable()) { join(); } }
};
void func5() {
    std::vector<std::thread> threads;
    for (int i = 0;i != 20;i++) {
        threads.push_back(std::thread(print_something, i));
    }
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
    //mem_fn 把成员函数转为函数对象，使用对象指针或对象(引用)进行绑定
    //与for_each搭配可实现对vector中的所有对象进行操作
}

template<typename Iterator, typename T>
struct accumulate_block {
    void operator()(Iterator frist, Iterator last, T& result) {
        result = std::accumulate(frist, last, result);
    }
};
template<typename Iterator, typename T>
T parallel_accmulate(Iterator first, Iterator last, T init) {
    long const length = std::distance(first, last);
    //计算两个迭代器表示的范围内包含元素的个数
    if (!length) { return init; }
    long const min_per_thread = 25;
    long const max_thread = (length + min_per_thread - 1) / min_per_thread;
    long const hardware_threads = std::thread::hardware_concurrency();
    long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_thread);
    long const block_size = length / num_threads;
    std::vector<T> results(num_threads);
    std::vector<std::thread> threads(num_threads - 1);
    Iterator block_start = first;
    for (long i = 0;i < (num_threads - 1);i++) {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        //将该迭代器前进或后退 n 个位置
        threads[i] = std::thread(accumulate_block<Iterator, T>(),
                                 block_start, block_end, std::ref(results[i]));
        block_start = block_end;
    }
    accumulate_block<Iterator, T>()(block_start, last, results[num_threads - 1]);
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
}

void func6() {
    std::thread::id master_thread;
    if (std::this_thread::get_id() == master_thread) {
        std::cout << "thread id:" << std::this_thread::get_id();
    }
    do_something_else();
}

int main() {
    func6();
}