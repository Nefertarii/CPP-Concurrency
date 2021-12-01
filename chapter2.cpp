#include "headfile.h"
//数据共享操作 互斥量mutex
//条件竞争(race condition)
//软件事务内存 STM

std::list<int> list1;
std::mutex mutex1;
//互斥访问的量 可理解为锁 在任意时刻最多允许一个线程对其进行上锁
//声明全局变量是为了保证能被多线程访问
void add_list(int value) {
    std::lock_guard<std::mutex> guard(mutex1);
    //lock_guard只有构造和析构函数 使用RAII
    //调用构造函数时会自动加锁调用lock() 析构时会自动解锁调用unlock()
    list1.push_back(value);
}
bool list_contains(int value) {
    std::lock_guard<std::mutex> guard(mutex1);
    return std::find(list1.begin(), list1.end(), value) != list1.end();
}

class data1 {
    int i;
    std::string str;
public:
    void change(std::string str_, int n) {
        str = str_;
        i = n;
    }
    void do_something() { std::cout << str <<":"<< i << "\n"; }
};
class data1_wrapper {
    data1 data;
    std::mutex mtx;
public:
    data1_wrapper() {}
    template<typename Func>
    void process_data(Func func) {
        std::lock_guard<std::mutex> lock(mtx);
        func(data);
    }
};
data1* unprotected;
void malicious_func(data1& protected_data) {
    protected_data.change("malicious_func", 20);
    unprotected = &protected_data;
}
void func1() {
    data1_wrapper x;
    x.process_data(malicious_func);
    unprotected->do_something();
    //调用函数func1能够绕过保护机制将函数malicious_function传递进去
    //在没有锁定互斥量的情况下调用do_something()
}

/*
template<typename T, typename Container=std::queue<T>>
class stack {
public:
    explicit stack(const Container&);
    explicit stack(Container && = Container());
    template <class Alloc> explicit stack(const Alloc&);
    template <class Alloc> stack(const Container&, const Alloc&);
    template <class Alloc> stack(Container&&, const Alloc&);
    template <class Alloc> stack(stack&&, const Alloc&);
    bool empty() const;
    size_t size() const;
    T& top();
    T const& top() const;
    void push(T const&);
    void push(T&&);
    void pop();
    void swap(stack&&);
    template <class... Args> void emplace(Args&&... args);
    //c++14
};*/

struct empty_stack: std::exception {
    const char* what() const throw() { return "empty stack!"; }
};
template<typename T>
class thread_safe_stack {
private:
    std::stack<T> data;
    mutable std::mutex mtx;
public:
    thread_safe_stack() :data(std::stack<T>()) {}
    thread_safe_stack(const thread_safe_stack& other) {
        std::lock_guard<std::mutex> lock(other.mtx);
        data = other.data;
    }
    thread_safe_stack& operator=(const thread_safe_stack&) = delete;
    void push(T value) {
        std::lock_guard<std::mutex> lock(mtx);
        data.push(value);
    }
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> lock(mtx);
        if (data.empty()) throw empty_stack();
        std::shared_ptr<T> const res(std::make_shared<T>(data.top()));
        data.pop();
        return res;
    }
    void pop(T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        if (data.empty()) throw empty_stack();
        value = data.top();
        data.pop();
    }
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return data.empty();
    }
    //削减接口可以获得最大程度的安全,甚至限制对栈的一些操作
    //使用std::shared_ptr可以避免内存分配管理的问题,并避免多次使用new和delete操作
    //简化接口更有利于数据控制，可以保证互斥量将一个操作完全锁住
};

class big_class {
private:
    int num;
    std::string str;
public:
    void do_something() { std::cout << str << ":" << num << "\n"; }
    void change(std::string str_, int num_) {
        str = str_;
        num = num_;
    }
    big_class() = default;
    big_class(big_class const& rhs) {
        num = rhs.num;
        str = rhs.str;
    }
    big_class& operator=(big_class const& rhs) {
        num = rhs.num;
        str = rhs.str;
        return *this;
    }
};
void swap(big_class& lhs, big_class& rhs) {
    std::cout << "swap\n";
    big_class tmp(lhs);
    lhs = rhs;
    rhs = tmp;
}
class X {
private:
    big_class detail;
    std::mutex mtx;
public:
    X(big_class const& detail_) :detail(detail_) {}
    friend void swap(X& lhs, X& rhs) {
        /*
        if (&lhs == &rhs) { return; }
        std::lock(lhs.mtx, rhs.mtx);
        std::lock_guard<std::mutex> lock_left(lhs.mtx, std::adopt_lock);
        std::lock_guard<std::mutex> lock_right(rhs.mtx, std::adopt_lock);
        std::cout << "lock_guard\n";
        //调用std::lock()锁住两个互斥量
        //并且创建两个std:lock_guard实例
        //使用adopt_lock表示初始化时互斥量已经加锁，通知lock_guard不需要再构造函数中lock这个互斥量了
        //unique_lock使用时同理，在添加该参数时互斥量需要已经lock
        swap(lhs.detail, rhs.detail);
        */

        if (&lhs == &rhs) { return; }
        std::scoped_lock<std::mutex, std::mutex> guard(lhs.mtx, rhs.mtx);
        //std::scoped_lock同样可以同时锁住两个互斥量
        //创建scoped_lock时，它试图取得给定互斥的所有权。
        //控制离开scoped_lock对象的作用域时，析构scoped_lock并以逆序释放互斥
        //若给出数个互斥，则使用免死锁算法
        //std::scoped_lock guard(lhs.mtx, rhs.mtx); //c++17 同时利用了c++17隐式推导机制
        std::cout << "scoped_lock\n";
        swap(lhs.detail, rhs.detail);
    }
    void do_something() { detail.do_something(); }
};
void func2() {
    big_class c1, c2;
    c1.change("one", 1);
    c2.change("two", 2);
    X x1(c1), x2(c2);
    x1.do_something();
    x2.do_something();
    swap(x1, x2);
    x1.do_something();
    x2.do_something();
}


class hierarchical_mutex {
private:
    std::mutex internal_mutex; //内部锁
    Ulong const hierarchy_value; //层次量
    Ulong previous_hierarchy_value;
    static thread_local Ulong this_thread_hierarchy_value;
    //thread_local具有线程周期 线程开始时生成结束时销毁
    //和static一样 同一线程的该类回共享一个变量且只在第一次执行时初始化
    void check_hierarchy_violation() {
        if (this_thread_hierarchy_value <= hierarchy_value) {
            throw std::logic_error("mutex hierarchy violated");
        }
    }
    void update_hierarchy_value() {
        previous_hierarchy_value = this_thread_hierarchy_value;
        this_thread_hierarchy_value = hierarchy_value;
    }
public:
    explicit hierarchical_mutex(Ulong value) :
        hierarchy_value(value), previous_hierarchy_value(0) {}
    void lock() {
        check_hierarchy_violation();
        internal_mutex.lock();
        update_hierarchy_value();
    }
    void unlock() {
        if (this_thread_hierarchy_value != hierarchy_value) {
            throw std::logic_error("mutex hierarchy violated");
        }
        this_thread_hierarchy_value = previous_hierarchy_value;
        internal_mutex.unlock();
    }
    bool try_lock() {
        check_hierarchy_violation();
        if (!internal_mutex.try_lock()) {
            return false;
        }
        update_hierarchy_value();
        return true;
    }
};
thread_local Ulong hierarchical_mutex::this_thread_hierarchy_value(ULONG_MAX);

hierarchical_mutex high_level_mutex(10000);
hierarchical_mutex low_level_mutex(5000);
hierarchical_mutex other_mutex(6000);
int low_level_stuff() {
    std::cout << "low level\n";
    std::lock_guard<hierarchical_mutex> lk(low_level_mutex);
    return 5000;
}
int high_level_stuff() {
    std::cout << "high level\n";
    std::lock_guard<hierarchical_mutex> lk(high_level_mutex);
    return 10000;
}
int other_level_stuff() {
    std::cout << "other level\n";
    std::lock_guard<hierarchical_mutex> lk(high_level_mutex);
    return 6000;
}
void thread_a() { high_level_stuff(); }
void thread_b() {
    std::lock_guard<hierarchical_mutex> lk(other_mutex);
    other_level_stuff();
}

class X2 {
private:
    big_class detail;
    std::mutex mtx;
public:
    X2(big_class const& rhs) :detail(rhs) {}
    friend void swap(X2& lhs, X2& rhs) {
        if (&lhs == &rhs) { return; }
        std::unique_lock<std::mutex> lock_l(lhs.mtx, std::defer_lock);
        std::unique_lock<std::mutex> lock_r(rhs.mtx, std::defer_lock);
        //unique_lock更灵活 但性能比lock_guard低
        //该对象可调用std::move移动,但不可赋值
        //使用adopt_lock表示初始化时互斥量没有加锁，且通知lock_guard不要在构造函数中lock这个互斥量
        //unique_lock使用时同理，在添加该参数时互斥量不能提前lock
        std::lock(lock_l, lock_r);
        std::cout << "unique_lock\n";
        swap(lhs.detail, rhs.detail);
    }
    void do_something() { detail.do_something(); }
};
void func3() {
    big_class c1, c2;
    c1.change("one", 1);
    c2.change("two", 2);
    X2 x1(c1), x2(c2);
    x1.do_something();
    x2.do_something();
    swap(x1, x2);
    x1.do_something();
    x2.do_something();
}

std::unique_lock<std::mutex> get_lock() {
    std::mutex mtx;
    std::unique_lock<std::mutex> lk(mtx);
    std::cout << "prepare\n";
    return lk;
}
void func4() {
    std::unique_lock<std::mutex> lk(get_lock());
    lk.unlock();
    do_something();
}

//一个给定操作需要两个或两个以上的互斥量时,将出现潜在的问题:死锁
//与条件竞争完全相反,死锁导致不同的两个线程会互相等待,从而什么都没做
//使用std::lock——可以一次性锁住多个(两个以上)的互斥量
//避免嵌套锁：一个线程已获得一个锁时，别再去获取第二个
//避免在持有锁时调用用户提供的代码：用户程序可能做任何事情，包括获取锁
//使用固定顺序获取锁：定义遍历的顺序，一个线程必须先锁住A才能获取B的锁
//在锁住B之后才能获取C的锁，且不允许反向遍历
//使用层次结构的锁
//如果很多线程正在等待同一个资源,当有线程持有锁的时间过长,这就会增加等待的时间
//可能的情况下,锁住互斥量的同时只能对共享数据进行访问
//试图对锁外数据进行处理,特别是做一些费时的动作,比如:对文件的输入/输出操作进行上锁,这样多线程带来的性能效益会被抵消。

class Y {
private:
    int detail;
    mutable std::mutex mtx;
    int get_detail()const {
        std::lock_guard<std::mutex> lock_a(mtx);
        return detail;
    }
public:
    Y(int n) :detail(n) {}
    friend bool operator==(Y const& lhs, Y const& rhs) {
        if (&lhs == &rhs) { return true; }
        int const lhs_value = lhs.get_detail();
        int const rhs_value = lhs.get_detail();
        return lhs_value == rhs_value;
    }
};

std::shared_ptr<int> ptr;
std::once_flag resource_flag;
void init_resource() { ptr.reset(new int(10)); }
void func5() {
    std::call_once(resource_flag, init_resource);
    print_something(*ptr);
}
//利用call_once,和once_flag来进行延迟初始化，能避免竞争同时减少消耗
class X3 {
private:
    std::string detail;
    std::string connect;
    std::once_flag connect_init_flag;
    void open_connection() {
        connect = "open connect:" + detail;
        std::cout << connect << "\n";
    }
public:
    X3(std::string const& detail_) :detail(detail_) {}
    void send_data(std::string const& data) {
        std::call_once(connect_init_flag, &X3::open_connection, *this);
        //call_once若需要成员函数初始化,则还需要将this指针传入
        std::cout << "Send:" << data << "\n";
    }
    std::string receive_data() {
        std::call_once(connect_init_flag, &X3::open_connection, *this);
        return "get_receive";
    }
};
void func6() {
    X3 S1("server 1"), S2("server 2");
    S1.send_data("send once");
    std::cout << S1.receive_data() << "\n";
}
//call_once在面对static多线程初始化时,可以这样调用函数初始化,避免数据竞争

//class dns_entry;
class dns_cache {
private:
    std::map<std::string, std::string> entries;
    mutable std::shared_mutex entry_mutex;
public:
    std::string find_entry(std::string const& domain) const {
        std::shared_lock<std::shared_mutex> lk(entry_mutex);
        std::map<std::string, std::string>::const_iterator const it = entries.find(domain);
        return (it == entries.end() ? std::string() : it->second);
    }
    void update_entry(std::string const& domain, std::string const& dns_details) {
        std::lock_guard<std::shared_mutex> lk(entry_mutex);
        entries[domain] = dns_details;
    }
};
void func7() {
    
}
//find_entry使用shared_lock来保护共享和只读数据,使得多线程可同时调用且不会出错
//update_entry使用lock_guard在表格需要更新时为其提供独占访问权
//并阻止其他线程修改数据或调用find_entry;

//大多数情况当需要嵌套锁时,就要对代码设计进行改动
//嵌套锁一般用在可并发访问的类上,所以使用互斥量保护其成员数据
//每个公共成员函数都会对互斥量上锁,然后完成对应的操作后再解锁互斥量
//有时成员函数会调用另一个成员函数,这种情况下,第二个成员函数也会试图锁住互斥量,这就会导致未定义行为的发生
//“变通的”解决方案会将互斥量转为嵌套锁,第二个成员函数就能成功的进行上锁,并且函数能继续执行。
//std::recursive_mutex可对同一线程的单个实例上获取多个锁
//该互斥量在锁住其他线程前必须释放所拥有的所有锁,lock三次,就需要unlock三次

int main() {
    func7();
}