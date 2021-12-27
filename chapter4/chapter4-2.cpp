
#include "../headfile.h"

//获取-释放操作序列(内存模型)

std::atomic<bool> atomic_x, atomic_y;
std::atomic<int> atomic_int;
void rel_write_x() {
    atomic_x.store(true, std::memory_order_release);
}
void rel_write_y() {
    atomic_y.store(true, std::memory_order_release);
}
void acq_read_x_y() {
    while (!atomic_x.load(std::memory_order_acquire)) { ; }
    if (atomic_y.load(std::memory_order_acquire)) { ++atomic_int; }
}
void acq_read_y_x() {
    while (!atomic_y.load(std::memory_order_acquire)) { ; }
    if (atomic_x.load(std::memory_order_acquire)) { ++atomic_int; }
}
void func1() {
    atomic_x = false;
    atomic_y = false;
    atomic_int = 0;
    std::thread T1(rel_write_x);
    std::thread T2(rel_write_y);
    std::thread T3(acq_read_x_y);
    std::thread T4(acq_read_y_x);
    T1.join();
    T2.join();
    T3.join();
    T4.join();
    if (atomic_int.load() == 0) { std::cout << "atomic_int still equal 0\n"; }
    else { std::cout << "atomic_int now equal " << atomic_int.load() << "\n"; }
    //atomic_int可能会为0,因为可能在加载x和y的时候,读取到的是false
    //因为x和y是由不同线程写入,所以序列中的每一次释放-获取都不会影响到其他线程的操作
}

std::atomic<bool> atomic_x2, atomic_y2;
std::atomic<int> atomic_int2;
void write_x_y() {
    atomic_x2.store(true, std::memory_order_relaxed);
    atomic_y2.store(true, std::memory_order_release);
}
void read_y_x() {
    while (!atomic_y2.load(std::memory_order_acquire)) { ; }
    if (atomic_x2.load(std::memory_order_relaxed)) {
        ++atomic_int;
    }
}
void fun2() {
    atomic_x2 = false;
    atomic_y2 = false;
    atomic_int2 = 0;
    std::thread T1(write_x_y);
    std::thread T2(read_y_x);
    T1.join();
    T2.join();
    if (atomic_int2.load() == 0) { std::cout << "atomic_int still equal 0\n"; }
    else { std::cout << "atomic_int now equal " << atomic_int2.load() << "\n"; }
    //两次存储由一个线程来完成,当需要使用memory_order_release改变y中的存储,并且使用memory_order_acquire来加载y中的值
    //读取y时会得到true,和存储时写入的一样
    //因为存储使用的是memory_order_release,读取使用的是memory_order_acquire,存储就与读取就同步了
    //因为这两个操作是由同一个线程完成的,所以存储x先行于加载y
    //对y的存储同步与对y的加载,存储x也就先行于对y的加载,并且扩展先行于x的读取
    //因此,加载x的值必为true,且atomic_int不会为0
    //如果对于y的加载不是在while循环中,情况可能就会有所不同
    //加载y的时候可能会读取到false,这种情况下对于读取到的x是什么值,就没有要求了
    //为了保证同步,加载和释放操作必须成对,释放操作存储的值必须要让获取操作看到
    //当存储如atomic_y2.store或加载如atomic_y2.load,都是一个释放操作时,对x的访问就无序了
    //也就无法保证读到的是true,并且还会导致atomic_int为0
}
//获取-释放操作会影响序列中的释放操作

std::atomic<int> data[5];
std::atomic<bool> sync1(false), sync2(false);
void T1_func() {
    data[0].store(42, std::memory_order_relaxed);
    data[1].store(84, std::memory_order_relaxed);
    data[2].store(126, std::memory_order_relaxed);
    data[3].store(168, std::memory_order_relaxed);
    data[4].store(200, std::memory_order_relaxed);
    sync1.store(true, std::memory_order_release);
}
void T2_func() {
    while (!sync1.load(std::memory_order_acquire)) { ; }
    sync2.store(true, std::memory_order_relaxed);
}
void T3_func() {
    while (!sync2.load(std::memory_order_acquire)) { ; }
    //assert(data[0]==42) ...
    //代替了断言
    if (data[0].load(std::memory_order_relaxed) != 42) { std::cout << "data[0] error\n"; }
    else { std::cout << "data[0] ok\n"; }
    if (data[1].load(std::memory_order_relaxed) != 84) { std::cout << "data[0] error\n"; }
    else { std::cout << "data[1] ok\n"; }
    if (data[2].load(std::memory_order_relaxed) != 126) { std::cout << "data[0] error\n"; }
    else { std::cout << "data[2] ok\n"; }
    if (data[3].load(std::memory_order_relaxed) != 168) { std::cout << "data[0] error\n"; }
    else { std::cout << "data[3] ok\n"; }
    if (data[4].load(std::memory_order_relaxed) != 200) { std::cout << "data[0] error\n"; }
    else { std::cout << "data[4] ok\n"; }
}
void func3() {
    std::thread T1(T1_func);
    std::thread T2(T2_func);
    std::thread T3(T3_func);
    T1.join();
    T2.join();
    T3.join();
    //尽管T2只接触到变量syn1和sync2,首先T1将数据存储到data中,先行于存储sync1(它们在同一个线程内)
    //因为加载sync1的是一个while循环,它最终会看到T1存储的值(是从“释放 - 获取”对的后半对获取)
    //因此对于sync1的存储先行于最终对于sync1的加载(在while循环中)
    //T3的加载操作位于存储sync2操作的前面(也就是先行)
    //存储sync2因此先行于T3的加载,加载又先行于存储sync2
    //存储sync2又先行于加载sync2,加载syn2又先行于加载data
    //因此T1存储数据到data的操作先行于T3中对data的加载,所以可以保证断言都不会触发
}
//通过先行发生关系的可传递性,控制线程之间的同步操作

std::atomic<int> sync;
void T1_func2() {
    data[0].store(42, std::memory_order_relaxed);
    data[1].store(84, std::memory_order_relaxed);
    data[2].store(126, std::memory_order_relaxed);
    data[3].store(168, std::memory_order_relaxed);
    data[4].store(200, std::memory_order_relaxed);
    sync.store(true, std::memory_order_release);
}
void T2_func2() {
    int expected = 2;
    while (!sync.compare_exchange_strong(expected, 2, std::memory_order_acq_rel)) {
        expected = 1;
    }
}
void T3_func2() {
    while (sync.load(std::memory_order_acquire) < 2) { ; }
    //assert(data[0]==42) ...
    //代替了断言
    if (data[0].load(std::memory_order_relaxed) != 42) { std::cout << "data[0] error\n"; }
    else { std::cout << "data[0] ok\n"; }
    if (data[1].load(std::memory_order_relaxed) != 84) { std::cout << "data[0] error\n"; }
    else { std::cout << "data[1] ok\n"; }
    if (data[2].load(std::memory_order_relaxed) != 126) { std::cout << "data[0] error\n"; }
    else { std::cout << "data[2] ok\n"; }
    if (data[3].load(std::memory_order_relaxed) != 168) { std::cout << "data[0] error\n"; }
    else { std::cout << "data[3] ok\n"; }
    if (data[4].load(std::memory_order_relaxed) != 200) { std::cout << "data[0] error\n"; }
    else { std::cout << "data[4] ok\n"; }
}
void func4() {
    std::thread T1(T1_func);
    std::thread T2(T2_func);
    std::thread T3(T3_func);
    T1.join();
    T2.join();
    T3.join();
    //通过改用读-改-写(acq-rel)操作,将两个变量合并为了一个变量
    //序列一致的读-改-写操作行为就像同时使用了获取和释放的操作
    //序列一致的加载动作就像使用了获取语义的加载操作
    //并且序列一致的存储操作就如使用了释放语义的存储
}
//锁住互斥量是一个获取操作,解锁这个互斥量是一个释放操作
//随着互斥量的增多,必须确保同一个互斥量在读取变量或修改变量的时候是锁住的

struct X {
    int i;
    std::string str;
};
std::atomic<X*> p;
std::atomic<int> a;
void create_x() {
    X* x = new X;
    x->i = 42;
    x->str = "hello";
    a.store(99, std::memory_order_relaxed);
    p.store(x, std::memory_order_release);
}
void use_x() {
    X* x;
    while (!(x = p.load(std::memory_order_consume))) {
        sleep(100);
    }
    //memory_order_consume也是 获取-释放 序列模型的一部分
    if (x->i != 42) { std::cout << "x->i error\n"; }
    else { std::cout << "x->i ok\n"; }
    if (x->str != "hello") { std::cout << "x->str error\n"; }
    else { std::cout << "x->str ok\n"; }
    if (a.load(std::memory_order_relaxed) != 99) { std::cout << "a load error\n"; }
    else { std::cout << "a load ok\n"; }
}
void func5() {
    std::thread T1(create_x);
    std::thread T2(use_x);
    T1.join();
    T2.join();
}
//memory_order_consume很特别,它完全依赖于数据
//第二个操作依赖于第一个操作的结果,这样两个操作之间就有了数据依赖
//数据依赖分前序依赖(dependency-ordered-before)和携带依赖(carries-a-dependency-to)
//携带依赖对于数据依赖的操作,严格应用于一个独立线程和其基本模型
//如果A操作结果要使用操作B的操作数,而后A将携带依赖于B


//这种内存序列的一个很重要使用方式,在原子操作载入指向数据的指针时
//当使用memory_order_consume作为加载语义,并且memory_order_release作为之前的存储语义
//要保证指针指向的值是已同步的,并且不需要对其他任何非独立数据施加任何同步要求
std::atomic<X*> p2;
std::atomic<int> a2;
void create_x2() {
    X* x = new X;
    x->i = 42;
    x->str = "hello";
    a2.store(99, std::memory_order_relaxed);
    p2.store(x, std::memory_order_relaxed);
}
void use_x2() {
    X* x;
    while (!(x = p2.load(std::memory_order_consume))) {
        sleep(100);
    }
    if (x->i != 42) { std::cout << "x->i error\n"; }
    else { std::cout << "x->i ok\n"; }
    if (x->str != "hello") { std::cout << "x->str error\n"; }
    else { std::cout << "x->str ok\n"; }
    if (a2.load(std::memory_order_relaxed) != 99) { std::cout << "a2 load error\n"; }
    else { std::cout << "a2 load ok\n"; }
}
void func6() {
    std::thread T1(create_x2);
    std::thread T2(use_x2);
    T1.join();
    T2.join();
    //尽管对a2的存储在存储p2之前,并且存储p2的操作标记为memory_order_release
    //加载p2的操作标记为memory_order_consume,表示存储p2仅先行那些需要加载p2的操作
    //同样也意味着X结构体中数据成员所在的断言语句不会被触发
    //因为对x2变量操作的表达式对加载p2的操作携带有依赖。
    //另一方面,对于加载变量a2的断言就不能确定是否会被触发
    //这个操作并不依赖于p2的加载操作,所以没法保证数据已经被读取
}
//有时不想为携带依赖增加其他开销,想让编译器在寄存器中缓存这些值,以及优化重排序操作代码
//而不是对这些依赖大惊小怪,可以使用std::kill_dependecy()来显式打破依赖链
//std::kill_dependency()是一个简单的函数模板,会复制提供的参数给返回值
//例如,当你拥有一个全局的只读数组,当其他线程对数组索引进行检索时
//使用的是std::memory_order_consume,那么可以使用std::kill_dependency()让编译器知道这里不需要重新读取该数组的内容
/*
int global_data[]={…};
std::atomic<int> index;
void f() {
    int i=index.load(std::memory_order_consume);
    do_something_with(global_data[std::kill_dependency(i)]);
}
*/
//实际操作中应该持续使用memory_order_acquire
//而对于 memory_order_consume和std::kill_dependency的使用是没有必要

int main() {
    func6();
}