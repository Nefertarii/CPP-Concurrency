#include <atomic>
#include <iostream>

/* atomic 中一些常量
 * memory_order_relaxed
 * 宽松操作,没有同步或顺序制约,仅对此操作要求原子性
 * memory_order_consume
 * 有此内存顺序的加载操作,在其影响的内存位置进行消费操作
 * 当前线程中依赖于当前加载的该值的读或写不能被重排到此加载前
 * 其他释放同一原子变量的线程的对数据依赖变量的写入,为当前线程所可见
 * 在大多数平台上,这只影响到编译器优化
 * memory_order_acquire
 * 有此内存顺序的加载操作,在其影响的内存位置进行获得操作
 * 当前线程中读或写不能被重排到此加载前
 * 其他释放同一原子变量的线程的所有写入,能为当前线程所见
 * memory_order_release
 * 有此内存顺序的存储操作进行释放操作
 * 当前线程中的读或写不能被重排到此存储后
 * 当前线程的所有写入,可见于获得该同一原子变量的其他线程
 * 并且对该原子变量的带依赖写入变得对于其他消费同一原子对象的线程可见
 * memory_order_acq_rel
 * 带此内存顺序的读修改写操作既是获得操作又是释放操作
 * 当前线程的读或写内存不能被重排到此存储前或后
 * 所有释放同一原子变量的线程的写入可见于修改之前,而且修改可见于其他获得同一原子变量的线程
 * memory_order_seq_cst
 * 有此内存顺序的加载操作进行获得操作,存储操作进行释放操作,而读修改写操作进行获得操作和释放操作
 * 再加上存在一个单独全序,其中所有线程以同一顺序观测到所有修改
 */

//atomic中这些类型的所有操作都是原子的,不过也可以用互斥锁来模拟原子操作
//而标准原子类型的实现也是这样模拟出来的
//它们(几乎)都有一个is_lock_free()成员函数,
//这个函数可以查询某原子类型的操作是直接用的原子指令(返回true)
//还是内部用了一个锁结构(返回false)

//除开std::atomic_flag 这个布尔类型不提供is_lock_free()之外
//其他的类型都可以通过模板std::atomic<>得到原子类型
//标准原子类型是不能进行拷贝和赋值的,但是可以隐式转化成对应的内置类型,因此还是能赋值
//每种函数类型的操作都有一个内存排序参数，这个参数可以用来指定存储的顺序。5.3节中，会对存储顺序选项进行详述。现在，只需要知道操作分为三类：
//Store操作,可选如下顺序
//memory_order_relaxed, memory_order_release, memory_order_seq_cst
//Load操作,可选如下顺序
//memory_order_relaxed, memory_order_consume, memory_order_acquire, memory_order_seq_cst
//exchange(读-改-写)操作,可选如下顺序
//memory_order_relaxed, memory_order_consume, memory_order_acquire, 
//memory_order_release, memory_order_acq_rel, memory_order_seq_cst

void func1() {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    //atomic_flag总是需要被初始化
    //该对象声明为static时,初始化也要保证为静态初始化
    //这个对象只要三种操作
    flag.clear(std::memory_order_release);
    //将原子标志设置为 false
    bool x = flag.test_and_set();
    //将原子标志设置为 true,并返回其先前值
    flag.~atomic_flag();
    //销毁
}

void func2() {
    //atomic_bool的操作比atomic_flag将要更多,同时是无锁的
    std::atomic<bool> atomic_bool;
    bool x = atomic_bool.load(std::memory_order_acquire);
    atomic_bool.store(true);
    x = atomic_bool.exchange(false, std::memory_order_acq_rel);
    //exchange在当前值与预期值一致时,将储存新值
    //返回true时执行存储操作,false则更新期望值
    atomic_bool.compare_exchange_weak();
    //weak在原始值与预期值一致时,不一定会存储成功,允许出乎意料的返回
    atomic_bool.compare_exchange_strong();
    //strong在实际值与期望值不符时,能保证值返回false,内部有循环

    //通常利用一个循环来使用compare_exchange
    bool expected = false;
    extern std::atomic<bool> atomic_bool2;
    while (!atomic_bool2.compare_exchange_weak(expected, true) && !expected);
    //经历每次循环的时候,期望值都会重新加载,所以当没有其他线程同时修改期望时
    //strong和weak的调用都会在下一次(第二次)成功
    //compare_exchange_strong()就能保证值返回false。这就能消除对循环的需要
}




int main() {
    
}