#include "headfile.h"

#include <chrono>

//chrono与延时操作的搭配
//shared_future的使用

int thread_sleep(int n) {
    sleep(n);
    return 42;
}
void func1() {
    std::cout << "Thread run\n";
    //chrono可以用于在wait_for函数中等待
    //如等待期望值状态变为就绪2秒
    std::future<int> f = std::async(thread_sleep, 3);
    auto status = f.wait_for(Sec(2));
    if (status == std::future_status::ready) {
        std::cout << "Thread done, ready time less than 2s\n";
    } else if (status == std::future_status::timeout) {
        std::cout << "Thread not done, time more than 2s\n";
    } else {
        std::cout << "Thread not done, delay\n";
    }
}

std::condition_variable cv;
bool done;
std::mutex mtx1;
bool wait_loop() {
    auto const timeout = SteadyClock::now() + Sec(2);
    std::unique_lock<std::mutex> lk(mtx1);
    while (!done) {
        //后缀为_unitl的(等待函数的)变量会使用时间点
        if (cv.wait_until(lk, timeout) == std::cv_status::timeout) {
            std::cout << "func timeout!\n";
            break;
        }
    }
    return done;
    //具有超时功能检测的函数
}
void func2() {
    std::cout << "Func run...\n";
    if (wait_loop()) {
        std::cout << "loop return.\n";
    }
}
//使用超时的最简单方式,就是对一个特定线程添加一个延迟处理
//当这个线程无所事事时,就不会占用其他线程的处理时间
//std::this_thread::sleep_for()和std::this_thread::sleep_until(),就像一个简单的闹钟
//当线程因为指定时延而进入睡眠时,可使用sleep_for()唤醒,因指定时间点修眠的,可使用sleep_until唤醒
//有些事必须在指定时间范围内完成,所以这里的耗时就很重要,另一方面,sleep_until()允许在某个特定时间点将调度线程唤醒
//休眠只是超时处理的一种形式,超时可以配合条件变量和期望值一起使用,超时甚至可以在尝试获取互斥锁时(当互斥量支持超时时)使用
//std::mutex和std::recursive_mutex都不支持超时锁,但是std::timed_mutex和std::recursive_timed_mutex支持
//这两种类型也有try_lock_for()和try_lock_until()成员函数,可以在一段时期内尝试获取锁或在指定时间点前获取互斥锁

//pure function 指函数对于相同的输入,永远会得到相同的输出,且没有太多副作用
//函数化编程(FP) 中的函数结果只依赖于传入的参数,不依赖外部状态
template<typename T>
std::list<T> quick_sort1(std::list<T> input) {
    if (input.empty()) { return input; }
    std::list<T> result;
    result.splice(result.begin(), input, input.begin());
    //splice 可以将从一个 list 转移元素给另一个中,共有三种用法
    //1(pos, other), 2(pos, other, it), 3(pos, other, it1, it2)
    //1从list类型的other转移所有元素到 *this 中,元素被插入到 pos 所指向的元素之前,操作后容器 other 变为空
    //2从other转移 it 所指向的元素到 *this 。元素被插入到 pos 所指向的元素之前。
    //3从other转移范围 [first, last) 中的元素到 *this 。元素被插入到 pos 所指向的元素之前。若 pos 是范围 [first,last) 中的迭代器则行为未定义。
    T const& pivot = *(result.begin());
    auto divide_point = std::partition(input.begin(), input.end(),
                                       [&](T const& t) {return t < pivot;});
    //partition在algorithm中 返回为一个正向迭代器
    //该函数前两个参数 [begin,end) 为范围, 第三个参数为筛选规则
    //返回的迭代器为两组数据的分界处(第二组数据的第一个元素)
    std::list<T> low;
    low.splice(low.end(), input, input.begin(), divide_point);
    auto low_tmp(quick_sort1(std::move(low)));
    auto high_tmp(quick_sort1(std::move(input)));
    result.splice(result.end(), high_tmp);
    result.splice(result.begin(), low_tmp);
    return result;
}


//future虽然可移动,但只能有一个实例可以获取结果
//shared_future则可以拷贝,因此可供多个对象引用同一关联期望值
//shared_future在多线程访问时是不同步的,需要锁来避免数据竞争




int main() {
    std::list<int> list1 = { 1, 8, 7, 2, 6, 2, 3, 4 };
    auto list2 = quick_sort1<int>(list1);
    for (auto i : list2) { std::cout << i << " "; }
}