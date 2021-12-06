#include "headfile.h"
//condition_variable条件变量的使用
//future使用
//promise与future的搭配操作

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

int find_some() {
    std::cout << "find for future...\n";
    sleep(1);
    std::cout << "answer is 42!\n";
    return 42;
}
void func2() {
    std::future<int> answer = std::async(find_some);
    sleep(2);
    std::cout << "future return " << answer.get() << "\n";
}

struct X {
    X() { std::cout << "X construct.\n"; }
    X(X&&) { std::cout << "X copy construct\n"; }
    X(X const&) { std::cout << "X copy construct\n"; }
    void foo(int n, std::string const& str) { std::cout << str << " " << n << "\n"; }
    std::string bar(std::string const& str) {
        std::cout << str << "\n";
        return str;
    }
    X& operator=(X&&) {
        std::cout << "X operator=\n";
        return *this;
    }
    X& operator=(X const&) {
        std::cout << "X operator=\n";
        return *this;
    }
    //类中的拷贝和拷贝构造等都省略了copy操作
};
struct Y {
    Y() { std::cout << "Y construct.\n"; }
    Y(Y&&) { std::cout << "Y copy construct\n"; }
    Y(Y const&) { std::cout << "Y copy construct\n"; }
    double operator()(double d) {
        std::cout << "operator() get double:" << d << "\n";
        return d;
    }
    Y& operator=(Y&&) {
        std::cout << "Y operator=\n";
        return *this;
    }
    Y& operator=(Y const&) {
        std::cout << "Y operator=\n";
        return *this;
    }
};
X baz(X& x) {
    std::cout << "copy X\n";
    return x;
};
class move_only {
public:
    move_only() { std::cout << "move_only construct\n"; }
    move_only(move_only&&) { std::cout << "move_only copy construct\n"; }
    move_only(move_only const&) = delete;
    move_only& operator=(move_only&&) {
        return *this;
    }
    move_only& operator=(move_only const&) = delete;
    void operator()() { std::cout << "move_only operator()\n"; }
};
X x;
Y y;
void func3() {
    sleep(1);
    auto f1 = std::async(&X::foo, &x, 42, "hello");
    //调用了x->foo

    sleep(1);
    auto f2 = std::async(&X::bar, x, "goodbye");
    //利用x的拷贝,调用了一个临时的x->bar
    sleep(1);
    std::cout << "f2 Got return:" << f2.get() << "\n";

    sleep(1);
    auto f3 = std::async(Y(), 3.1415);
    //利用Y的移动构造函数,调用了一个临时的y(3.1415);
    sleep(1);
    std::cout << "f3 Got return:" << f3.get() << "\n";

    sleep(1);
    auto f4 = std::async(std::ref(y), 2.7182);
    //调用了y(2.7182);
    sleep(1);
    std::cout << "f4 Got return:" << f4.get() << "\n";

    sleep(1);
    auto f5 = std::async(baz, std::ref(x));

    sleep(1);
    auto f6 = std::async(move_only());
    //通过std::move(move_only()),调用了一个临时的move_only()
}

void func4() {
    sleep(1);
    auto f6 = std::async(std::launch::async, Y(), 1.2345);
    auto f7 = std::async(std::launch::deferred, baz, std::ref(x));
    sleep(1);
    auto f8 = std::async(std::launch::deferred | std::launch::async, baz, std::ref(x));
    //std::launch::async 表示函数必须保证异步行为，即传递函数将在单独的线程中执行
    //std::launch::deferred 表示函数在wait()或get()调用时执行
    //std::launch::deferred|std::launch::async 为默认情况,取决于系统的负载,且无法控制
    sleep(1);
    f7.wait();
}

std::mutex mtx;
std::deque<std::packaged_task<void()>> tasks;
bool echo_shutdown = false;
void process_echo_message(int i) {
    std::cout << "thread sleep:" << i << " sec.\n";
    sleep(1);
}
void echo_thread() {
    int time = 1;
    while (!echo_shutdown) {
        process_echo_message(time);
        time++;
        std::packaged_task<void()> task;
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (tasks.empty()) { continue; }
            task = std::move(tasks.front());
            tasks.pop_front();
        }
        task();
    }
    std::cout << "thread shutdown run:" << time << " sec.\n";
}

template<typename Func>
std::future<void> post_task(Func f) {
    std::packaged_task<void()> task(f);
    std::future<void> res = task.get_future();
    std::lock_guard<std::mutex> lk(mtx);
    tasks.push_back(std::move(task));
    return res;
}
void func5() {
    std::thread echo_bg_thread(echo_thread);
    post_task(do_something);
    post_task(do_something);
    post_task(do_something);
    std::future<void> ret = post_task(ret_num);
    std::cout << "thread return: ";
    sleep(8);
    echo_shutdown = true;
    std::cout << "main thread send shutdown.\n";
    echo_bg_thread.join();
    //线程循环直到收到一条关闭的信息后关闭,当队列中没有任务,它将再次循环
    //除非它能在队列中提取出一个任务,然后释放队列上的锁,并且执行任务
    //这里,期望值与任务相关,当任务执行完成时,其状态会被置为“就绪”状态
    //post_task可以提供一个打包好的任务,可以通过这个任务调用get_future()成员函数获取期望值对象
    //并且在任务被推入列表之前,期望值将返回调用函数
}

struct connect_set {
    std::string status;
    std::vector<std::string> data;
    std::promise<std::string> prom;
};
void process_connect(connect_set connect) {
    connect.data.push_back("connect");
    connect.data.push_back("h");
    connect.data.push_back("e");
    connect.data.push_back("l");
    connect.data.push_back("l");
    connect.data.push_back("o");
    connect.data.push_back("disconnect");
    for (auto it = connect.data.begin(); it != connect.data.end(); it++) {
        sleep(1);
        std::string data = *it;
        std::cout << *it << "\n";
        if (*it == "disconnect") {
            connect.prom.set_value("disconnect");
            break;
            //set_value只能调用一次 多次调用会抛出异常
        }
    }
    
}
void func6() {
    connect_set net1;
    std::future<std::string> status = net1.prom.get_future();
    std::thread t(process_connect, std::move(net1));
    //需要使用std::move才能传入
    t.detach();
    std::string str = status.get();
    //调用后阻塞当前线程直到设置共享值
    std::cout << "Success get " << str << "\n";
}

void calculate(std::promise<double> prom, int n) {
    try {
        double ret = square(n);
        std::cout << "Normal\n";
        prom.set_value(ret);
    } catch (const std::exception& e) {
        std::cout << "Catch exception\n" << e.what() << "\n";
        prom.set_exception(std::current_exception());
    }
}
void func7() {
    std::promise<double> prom;
    std::future<double> result = prom.get_future();
    std::thread t2(calculate, std::move(prom), -10);
    t2.join();
    std::cout << "Get return: " << result.get() << "\n";
}

int main() {
    func7();
}