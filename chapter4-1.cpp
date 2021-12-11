#include "headfile.h"

//内存模型
//atomic原子操作

//C++标准定义类对象为:"存储区域",像int或float这样的对象是基本类型,也有用户定义类的实例
//无论对象是怎么样的类型,对象都会存储在一个或多个内存位置上
//对于C++多线程来说,因为所有东西都在内存中
//当两个线程访问不同的内存位置时,不会存在任何问题,一切都工作顺利。
//当两个线程访问同一个内存位置,如果没有线程更新数据,只读数据不需要保护或同步
//而当有线程对内存位置上的数据进行修改,那就有可能会产生条件竞争
//每个C++程序中的对象,都有(由程序中的所有线程对象)确定好的修改顺序,且在初始化开始阶段确定
//大多数情况下,这个顺序不同于执行中的顺序,但在给定的程序中,所有线程都需要遵守这个顺序

class spinlock_mutex {
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    spinlock_mutex(){}
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire));
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }
};//利用atomic_flag实现的自旋锁


struct Foo {
    int num;
    Foo(int num_) :num(num_) {};
};
void func1() {
    Foo array[5] = { 1, 2, 3, 4, 5 };
    std::atomic<Foo*> atomic_Foo(array);
    //在存储地址上做原子加法和减法，为+=, -=, ++, --提供简易的封装
    
    Foo* X = atomic_Foo.fetch_add(2);
    //atomic_Foo此时指向第三个元素,并返回原始值(array)
    if (X == array) { std::cout << "X same to array\n"; }
    else { std::cout << "X not same to array\n"; }

    if (atomic_Foo.load() == &array[2]) { std::cout << "true\n"; }
    else { std::cout << "array[2] is " << &array[2] << "\n"; }

    X = (atomic_Foo -= 1);
    //atomic_Foo指针减一,返回操作之后的值(array[1])
    if (X == &(array[1])) { std::cout << "true\n"; }
    else { std::cout << "X is " << X << "\n"; }

    if(atomic_Foo.load() == &(array[1])) { std::cout << "true\n"; }
    else { std::cout << "array[1] is " << &array[1] << "\n"; }

    X = (atomic_Foo += 3);
    //atomic_Foo指针减一,返回操作之后的值(array[4])
    if (X == &(array[4])) { std::cout << "true\n"; }
    else { std::cout << "X is " << X << "\n"; }
}

std::vector<int> data;
std::atomic<bool> ready(false);
void thread_read() {
    while (!ready.load()) {
        std::cout << "sleep....\n";
        sleep(1);
    }
    std::cout << "Read: " << data[0] << "\n";
}
void thread_write(int n) {
    data.push_back(n);
    ready = true;
}
void func2() {
    thread_write(42);
    std::thread T(thread_read);
    T.detach();
    //写入数据thread_write在ready标志就绪之前发生
    //ready.load()在读取数据之前
    //当ready为true时,写操作就与读操作同步,建立一个先行发生关系
    //因为先行发生可以传递,写入数据data.push_back()在ready标志就绪之前
    //此时的顺序写是必然在读之前的,因此不会发生错误
}
//同步发生:原子写操作W对变量x进行标记,同步与对x进行原子读操作
//读取的是W操作写入的内容;或是W之后同一线程上的原子写操作对x写入的值;
//亦或是任意线程对x的一系列原子读 - 改 - 写操作(如fetch_add())
//这里第一个线程所读取到的值为W操作写入
//先行发生:一个程序中基本构建块的操作顺序,它指定了某个操作去影响另一个操作
//对于单线程来说,当一个操作排在另一个之后,那么这个操作就是先行执行的
//如果源码中操作A发生在操作B之前,那么A就先行于B发生


void foo(int a, int b) {
    std::cout << "a:" << a << ", b:" << b << "\n";
}
void get_num(std::promise<int> x,std::promise<int> y) {
    static int i = 0;
    x.set_value(++i);
    y.set_value(++i);
}
void func3() {
    std::promise<int> P1, P2;
    auto F1 = P1.get_future();
    auto F2 = P2.get_future();
    std::thread T1(get_num, std::move(P1), std::move(P2));
    T1.detach();
    sleep(1);
    int num1 = F1.get();
    int num2 = F2.get();
    foo(num1, num2);
    //据说会根据编译器的不同,生成的汇编码导致函数输出不一样的结果
    //我还没遇到过,可能现代编译器已经修复了
}

std::atomic<bool> atomic_x, atomic_y;
std::atomic<int> atomic_int;
void write_x() {
    atomic_x.store(true, std::memory_order_seq_cst);
    //原子类型的操作可以指定6种模型的中的一种,用来控制同步以及对执行序列的约束
}
void write_y() {
    atomic_y.store(true, std::memory_order_seq_cst);
}
void read_x_y() {
    while (!atomic_x.load(std::memory_order_seq_cst)) { 
        std::cout << "atomic_x is true now.\n";
    }
    if (atomic_y.load(std::memory_order_seq_cst)) { ++atomic_int; }
}
void read_y_x() {
    while (!atomic_y.load(std::memory_order_seq_cst)) {
        std::cout << "atomic_y is true now.\n";
    }
    if (atomic_x.load(std::memory_order_seq_cst)) { ++atomic_int; }
}
void func4() {
    atomic_x = false;
    atomic_y = false;
    atomic_int = 0;
    std::thread T1(write_x);
    std::thread T2(write_y);
    std::thread T3(read_x_y);
    std::thread T4(read_y_x);
    T1.join();
    T2.join();
    T3.join();
    T4.join();
    if (atomic_int.load() == 0) { std::cout << "atomic_int still echo 0\n"; }
    else { std::cout << "atomic_int now echo " << atomic_int.load(); }
    //任何情况下 int都不能是0,因为不是存储x的操作发生,就是存储y的操作发生。
    //如果在read_x_then_y中加载y返回false,是因为存储x的操作肯定发生在存储y的操作之前
    //在这种情况下在read_y_then_x中加载x必定会返回true,因为while循环能保证在某一时刻y是true
    //因为memory_order_seq_cst的语义需要一个全序将所有操作都标记为memory_order_seq_cst
    //这就暗示着 加载y并返回false 与 存储y 的操作需要有一个确定的顺序
    //只有在全序时,当一个线程看到x == true,随后又看到y == false,这就意味着在总序列中存储x的操作发生在存储y的操作之前。
    //当然,也有可能发生加载x的操作返回false,或强制加载y的操作返回true
    //这两种情况下,int都等于1,当两个加载操作都返回true,int就等于2
}



int main() {
    func4();
}