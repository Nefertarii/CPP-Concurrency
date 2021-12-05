#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <stdexcept>


//mutex类
//std::mutex mutex1; //基础mutex(互斥量)
//mutex提供了能独占所有权的特性 不支持递归上锁 
//mutex禁止拷贝 其初始化后的mutex是unlocked状态
//mutex类的成员函数
//lock()     锁住该mutex 线程调用后如果该mutex没有被锁住，则调用线程会锁住该mutex，拥有锁直到unlock
//                               如果当前mutex被其他线程锁住，则调用线程会被阻塞(会一直等待下去直至解锁)
//                               如果当前mutex已被调用线程锁住，则会产生死锁(deadlock)
//unlock()   解锁该mutex 释放该mutex的锁
//try_lock() 尝试锁住该mutex 线程调用后如果该mutex没有被锁住，则调用线程会锁住该mutex，拥有锁直到unlock
//                                   如果当前mutex被其他线程锁住，则调用线程返回false，且不会被阻塞
//                                   如果当前mutex已被调用线程锁住，则会产生死锁(deadlock)
//mutex示例代码
std::mutex mtx;
volatile int counter = 0;
void attempt_10k_increases() {
    for (int i = 0; i != 10000; i++) {
        if (mtx.try_lock()) {
            counter++;
            mtx.unlock();
        }
    }
}
void example() {
    std::thread threads[10];
    for (int i = 0; i != 10; i++) {
        threads[i] = std::thread(attempt_10k_increases);
    }
    for (auto& th : threads) {
        th.join();
    }
    std::cout << "successful increases of the counter " << counter << "\n";
}


//std::recursive_mutex mutex2; //可递归的mutex
//recursive_mutex类的特性大致与mutex一致 但是允许同一线程对该mutex多次上锁
//多次上锁后的解锁需要调用与锁相同次数的unlock
//std::recursive_mutex示例代码
class counter {
private:
    boost::recursive_mutex mutex;
    int count;

public:
    counter() : count(0) {}
    int add(int val) {
        std::recursive_mutex::scoped_lock scoped_lock(mutex);
        count += val;
        return count;
    }
    int increment() {
        std::recursive_mutex::scoped_lock scoped_lock(mutex);
        return add(1);
    }
};
counter c;
void change_count(void*) {
    std::cout << "count == " << c.increment() << std::endl;
}
void example2() {
    std::thread threads[10];
    for (int i = 0; i < 10; ++i)
        threads[i] = std::thread(change_count, 0);
    for (auto& th : threads) th.join();
    return 0;
}



//std::timed_mutex mutex3; //可定时的mutex
//timed_mutex类比mutex多出两个成员函数
//try_lock_for() 接受一个时间范围
//在线程调用后的这段时间内如果没有获得该mutex的锁，则会被阻塞
//                      如果其他线程释放了该mutex的锁，则该线程获得这个mutex的锁
//                      如果在直到指定时间结束(超时)还为获得该mutex的锁，则返回false
//try_lock_until() 接受一个时间点作为参数
//在线程调用后，指定时间点未到来之前如果线程没有获得锁，则会被阻塞住
//                               如果其他线程释放了该mutex的锁，则该线程获得这个mutex的锁
//                               如果超时，则返回false
//timed_mutex示例代码
std::timed_mutex time_mtx2;
void fireworks() {
    while (!time_mtx2.try_lock_for(std::chrono::milliseconds(200))) {
        std::cout << "-";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "*\n";
    time_mtx2.unlock();
}
void example3() {
    std::thread threads[10];
    for (int i = 0; i != 10; i++) {
        threads[i] = std::thread(fireworks);
    }
    for (auto& th : threads) {
        th.join();
    }
}

//std::recursive_timed_mutex mutex4; //可定时可递归的mutex



//lock类
//std::lock_guard guard;      //mutex RAII相关 多用对线程互斥量上锁
//std::unique_lock unique;    //和lock_guard功能相似 但更方便上锁和解锁控制
//std::lock_guard示例代码
std::mutex mtx3;
void print_even(int x) {
    if (x % 2 == 0)
        std::cout << x << " is even\n";
    else
        throw(std::logic_error("not even"));
}
void print_thread_id(int id) {
    try {
        std::lock_guard<std::mutex> lck(mtx3);
        print_even(id);
    } catch (std::logic_error&) {
        std::cout << "[exception caught]\n";
    }
}
void example4() {
    std::thread threads[10];
    for (int i = 0; i != 10; i++) {
        threads[i] = std::thread(print_thread_id, i + 1);
    }
    for (auto& th : threads) {
        th.join();
    }
}


//std::unique_lock示例代码
std::mutex mtx4;           //临界区的互斥锁
void print_block(int n, char c) {
    //cout的独占访问权限由lck的生命周期决定
    std::unique_lock<std::mutex> lck(mtx4);
    for (int i = 0; i < n; ++i) {
        std::cout << c;
    }
    std::cout << '\n';
}
void example4() {
    std::thread th1(print_block, 50, '*');
    std::thread th2(print_block, 50, '$');

    th1.join();
    th2.join();
}


//在进行网络编程时，我们常常见到同步(Sync)/异步(Async)，阻塞(Block)/非阻塞(Unblock)四种调用方式：
//同步：就是在发出一个功能调用时，在没有得到结果之前，该调用就不返回。也就是必须一件一件事做,等前一件做完了才能做下一件事。
//例如普通B/S模式（同步）：提交请求->等待服务器处理->处理完毕返回这个期间客户端浏览器不能干任何事
//阻塞：是指调用结果返回之前，当前线程会被挂起（线程进入非可执行状态，在这个状态下，CPU不会给线程分配时间片，即线程暂停运行）
//函数只有在得到结果之后才会返回。
//有人也许会把阻塞调用和同步调用等同起来，实际上是不同的。对于同步调用来说，很多时候当前线程还是激活的，只是从逻辑上当前函数没有返回而已。
//例如，我们在socket中调用recv函数，如果缓冲区中没有数据，这个函数就会一直等待，直到有数据才返回。而此时，当前线程还会继续处理各种各样的消息。

//对象的阻塞模式和阻塞函数调用
//对象是否处于阻塞模式和函数是不是阻塞调用有很强的相关性，但是并不是一一对应的。
//阻塞对象上可以有非阻塞的调用方式，我们可以通过一定的API去轮询状态，在适当的时候调用阻塞函数，就可以避免阻塞。
//而对于非阻塞对象，调用特殊的函数也可以进入阻塞调用。函数select就是这样的一个例子。
//1. 同步，就是调用一个功能，该功能没有结束前，一直等结果。
//2. 异步，就是调用一个功能，不需要知道该功能结果，该功能有结果后通知（回调通知）
//3. 阻塞，就是调用（函数），（函数）没有接收完数据或者没有得到结果之前，不会返回。
//4. 非阻塞，就是调用（函数），（函数）立即返回，通过select通知调用者。
//同步IO和异步IO的区别就在于：数据拷贝的时候进程是否阻塞
//阻塞IO和非阻塞IO的区别就在于：应用程序的调用是否立即返回


int main() {
    example();
    return 0;
}