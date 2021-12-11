#include <atomic>
#include <iostream>

//atomic中这些类型的所有操作都是原子的,不过也可以用互斥锁来模拟原子操作
//而标准原子类型的实现也是这样模拟出来的
//它们(几乎)都有一个is_lock_free()成员函数,
//这个函数可以查询某原子类型的操作是直接用的原子指令(返回true)
//还是内部用了一个锁结构(返回false)

/* atomic中定义的6个顺序
 * memory_order_relaxed 各个CPU读取的值是未定义的,一个CPU在一个线程中修改一个值后,其他CPU不知道
 * memory_order_seq_cst 可理解为CPU的原子操作都是在一个线程上工作,一个修改后,其他CPU都会更新到新的值
 * acquire操作(load) —— memory_order_consume, memory_order_acquire
 * release操作(store) —— memory_order_release
 * acquire-release操作 —— memory_order_acq_rel (先acquire再做一次release)
 * 对于fetch_add这样的操作,既要load一下,又要store一下的,就用这个）
 * 一个原子做了acquire操作,读的是这个原子最后一次release操作修改的值。
 * 换句话说,要是采用release的方式store一个值,那么其他CPU都会看到这一次的修改。
 * memory_order_consume 只能保证当前的变量以及此线程中与之有依赖关系的变量,在其他CPU上都更新
 * 不能保证此线程中此load操作语句之前的其他变量的操作也被其他CPU看到
 * memory_order_acquire 不仅能保证当前变量的修改被其他CPU看到,也能保证这条语句之前的其他变量的load操作的值也被其他CPU看到
 */

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