#include "../headfile.h"

//内存模型
//atomic原子操作
//序列一致,松散型操作序列(内存模型)


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
    spinlock_mutex() {}
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

    if (atomic_Foo.load() == &(array[1])) { std::cout << "true\n"; }
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
void get_num(std::promise<int> x, std::promise<int> y) {
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
void seq_read_x_y() {
    while (!atomic_x.load(std::memory_order_seq_cst)) { ; }
    std::cout << "atomic_x now is false.\n";
    if (atomic_y.load(std::memory_order_seq_cst)) { ++atomic_int; }
}
void seq_read_y_x() {
    while (!atomic_x.load(std::memory_order_seq_cst)) { ; }
    std::cout << "atomic_x now is false.\n";
    if (atomic_x.load(std::memory_order_seq_cst)) { ++atomic_int; }
}
void func4() {
    atomic_x = false;
    atomic_y = false;
    atomic_int = 0;
    std::thread T1(write_x);
    std::thread T2(write_y);
    std::thread T3(seq_read_x_y);
    std::thread T4(seq_read_y_x);
    T1.join();
    T2.join();
    T3.join();
    T4.join();
    //assert(atomic_int.load!=0);
    //断言会在判断为假时触发
    if (atomic_int.load() == 0) { std::cout << "atomic_int still equal 0\n"; }
    else { std::cout << "atomic_int now equal " << atomic_int.load(); }
    //任何情况下 int都不能是0,因为不是存储x的操作发生,就是存储y的操作发生。
    //如果在read_x_then_y中加载y返回false,是因为存储x的操作肯定发生在存储y的操作之前
    //在这种情况下在read_y_then_x中加载x必定会返回true,因为while循环能保证在某一时刻y是true
    //因为memory_order_seq_cst的语义需要一个全序将所有操作都标记为memory_order_seq_cst
    //这就暗示着 加载y并返回false 与 存储y 的操作需要有一个确定的顺序
    //只有在全序时,当一个线程看到x == true,随后又看到y == false,这就意味着在总序列中存储x的操作发生在存储y的操作之前。
    //当然,也有可能发生加载x的操作返回false,或强制加载y的操作返回true
    //这两种情况下,int都等于1,当两个加载操作都返回true,int就等于2
}
//序列一致是最简单,直观的序列,但是也是花费最昂贵的内存序列
//它需要对所有线程进行全局同步,一个多处理器设备上,就需要处理期间在信息交换上耗费大量的时间


std::atomic<bool> atomic_x2, atomic_y2;
std::atomic<int> atomic_int2;
void relax_write_x_y() {
    atomic_x2.store(true, std::memory_order_relaxed);
    atomic_y2.store(true, std::memory_order_relaxed);
}
void relax_read_y_x() {
    while (!atomic_y2.load(std::memory_order_relaxed)) { ; }
    std::cout << "atomic_y2 now is false.\n";
    if (atomic_x2.load(std::memory_order_relaxed)) {
        ++atomic_int2;
    }
}
void func5() {
    atomic_x2 = false;
    atomic_y2 = false;
    atomic_int = 0;
    std::thread T1(relax_write_x_y);
    std::thread T2(relax_read_y_x);
    T1.join();
    T2.join();
    if (atomic_int2.load() == 0) { std::cout << "atomic_int2 still equal 0\n"; }
    else { std::cout << "atomic_int2 now equal " << atomic_int2.load(); }
    //此时的int可能会等于0
    //加载x的操作可能读取到false,即使加载y的操作读取到true,并且存储x的操作先于存储y的操作
    //x和y是两个不同的变量,所以这里没有顺序去保证每个操作产生相关值的可见性
}

std::atomic<int> atomic_int_x(0), atomic_int_y(0), atomic_int_z(0);
std::atomic<bool> ready2(false);
int const loop = 5;
struct Values {
    int x, y, z;
};
Values values1[loop];
Values values2[loop];
Values values3[loop];
Values values4[loop];
Values values5[loop];
void increment(std::atomic<int>* var, Values* values) {
    while (!ready2) { std::this_thread::yield(); }
    //yield让当前线程放弃执行,让操作系统调度另一线程继续执行
    //避免在ready2未准备好的情况下空循环 自旋锁
    for (int i = 0; i < loop; i++) {
        values[i].x = atomic_int_x.load(std::memory_order_relaxed);
        values[i].y = atomic_int_y.load(std::memory_order_relaxed);
        values[i].z = atomic_int_z.load(std::memory_order_relaxed);
        var->store(i + 1, std::memory_order_relaxed);
        std::this_thread::yield();
    }
}
void read_value(Values* values) {
    while (!ready2) { std::this_thread::yield(); }
    for (int i = 0; i < loop; i++) {
        values[i].x = atomic_int_x.load(std::memory_order_relaxed);
        values[i].y = atomic_int_y.load(std::memory_order_relaxed);
        values[i].z = atomic_int_z.load(std::memory_order_relaxed);
        std::this_thread::yield();
    }
}
void print_value(Values* values) {
    for (int i = 0; i < loop; i++) {
        if (i)
            std::cout << ",";
        std::cout << "(" << values[i].x << "," << values[i].y << "," << values[i].z << ")";
    }
    std::cout << std::endl;
}
void func6() {
    std::thread t1(increment, &atomic_int_x, values1);
    std::thread t2(increment, &atomic_int_y, values2);
    std::thread t3(increment, &atomic_int_z, values3);
    std::thread t4(read_value, values4);
    std::thread t5(read_value, values5);
    ready2 = true;
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    print_value(values1);
    print_value(values2);
    print_value(values3);
    print_value(values4);
    print_value(values5);
    //前三行中线程都做了更新(t1,t2,t3),后两行线程只是做读取(t4,t5)
}
//不同线程看到相同操作,不一定有着相同的顺序
//还有对于不同线程的操作,一个接着另一个执行的想法不再可行
//第一组值中x增1,第二组值中y增1,第三组中z增1
//x,y和z元素只在给定集中增加,但是增加是不均匀的,并且相对顺序在所有线程中都不同
//线程t3看不到x或y的任何更新,它能看到的只有z的更新,这并不妨碍别的线程观察z的更新,并同时观察x和y的更新
//任意组值都用三个变量保持一致,值从0到5依次递增,并且线程递增给定变量,所以打印出来的值在0到5的范围内都是合法的


int main() {
    func6();
}