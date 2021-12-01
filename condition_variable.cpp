#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>


//condition_variable类
//当某个condition_variable的wait函数被调用时，将使用std::unique_lock来锁住当前线程
//当前线程会一直阻塞，直到另外一个线程在相同的condition_variable对象上调用notification函数来唤醒当前线程
//condition_variable示例代码
std::mutex mtx; //全局锁
std::condition_variable cv; //全局条件变量
bool ready = false; //全局条件
void print_id(int id) {
    std::unique_lock<std::mutex> lck(mtx);
    while (!ready) { //等待条件为true
        cv.wait(lck); //阻塞线程
    }
    std::cout << "thread: " << id << "\n"; //线程被唤醒
}
void notifyall() {
    std::unique_lock<std::mutex> lck(mtx);
    ready = true; //设置全局条件为true
    cv.notify_all(); //唤醒所有线程
}
void example1() {
    std::thread threads[10];
    for (int i = 0; i < 10; ++i)
        threads[i] = std::thread(print_id, i);
    std::cout << "10 threads ready to race...\n";
    _sleep(5000);
    notifyall();
    for (auto& th : threads)
        th.join();
}

//示例结尾

//condition_variable有两种wait函数
//void wait(unique_lock<mutex>& lck);
//该wait函数被线程调用后将阻塞调用线程 并且线程获得锁，直到其他线程调用notify_... 函数唤醒当前线程
//在调用线程被阻塞时，该函数会自动使用lck.unlock释放锁，使得其他被阻塞在锁竞争上的线程可以继续执行
//调用线程一旦获得通知(notify)，wait函数也将自动使用lck.lock，使得lck的状态与wait函数被调用时相同

//template <class Predicate>
//void wait(unique_lock<mutex>& lck, Predicate pred);
//该wait函数在设置了Predicate的情况下，只有当pred条件为false时才会阻塞当前线程
//并且在收到其他线程的通知后 只有当pred为true时才会解除阻塞
//(Predicate 谓词 指出对主语怎么样，做什么，是什么)，在计算机语言的环境下，谓词是指条件表达式的求值返回真或假的过程
//void wait(unique_lock<mutex>& lck, Predicate pred)示例代码
std::mutex mtx2;
std::condition_variable cv2;
int cargo = 0;
bool cargo_available() {
    return cargo != 0;
}
void consume(int n) {
    for (int i = 0; i != n; i++) {
        std::unique_lock<std::mutex> lck2(mtx2);
        cv2.wait(lck2, cargo_available);
        std::cout << "cargo:" << cargo << "\n";
        cargo = 0;
    }
}
void example2() {
    std::thread consumer_threads(consume, 10);
    for (int i = 0; i < 10; ++i) {
        while (cargo_available()) {
            std::this_thread::yield();
        }
        std::unique_lock<std::mutex> lck2(mtx);
        cargo = i + 1;
        cv2.notify_one();
    }
    consumer_threads.join();
}
//示例结尾

//condition_variable成员函数wait_for同样有两个
//template <class Rep, class Period>
//cv_status wait_for(unique_lock<mutex>& lck,const chrono::duration<Rep,Period>& rel_time);
//与wait类似，但waitfor可以指定一个时间段，在调用线程收到通知或指定时间rel_time超时之前
//调用线程都会处于阻塞状态，而一旦超时或接受到了通知，wait_for将返回，并且后续同wait()处理相似
//template <class Rep, class Period, class Predicate>
//bool wait_for(unique_lock<mutex>& lck,const chrono::duration<Rep, Period>& rel_time, Predicate pred);
//该重载版本的pred表示wait_for的条件，只有在pred条件为false时调用该wait才会阻塞调用线程
//并且在收到其他线程的通知后，只有在pred为true时才会解除阻塞调用线程
int value;
std::condition_variable cv3;
void read_value() {
    std::cin >> value;
    cv3.notify_one();
}
void example3() {
    std::cout << "Enter an integer: ";
    std::thread th(read_value);
    std::mutex mtx3;
    std::unique_lock<std::mutex> lck(mtx3);
    while (cv3.wait_for(lck, std::chrono::seconds(1)) == std::cv_status::timeout) {
        std::cout << ".";
        std::cout.flush();
    }
    std::cout << "\nEntered: " << value << "\n";
    th.join();
}

//condition_variable成员函数wait_until
//template <class Clock, class Duration>
//cv_status wait_until(unique_lock<mutex>& lck, const chrono::time_point<Clock,Duration>& abs_time);
//与wait_for类似，但wait_until可以指定一个时间点，在当前线程收到通知或指定的时间点abs_time超时之前，调用线程会一直处于阻塞状态
//一旦超时或收到了其他线程的通知，wait_until将返回,剩下的步骤与wait类似
//template <class Clock, class Duration, class Predicate>
//bool wait_until(unique_lock<mutex>& lck, 
//                 const chrono::time_point<Clock,Duration>& abs_time, Predicate pred);
//作为wait_until的重载版本，最后一个参数pred表示wait_until的条件，
//只有在pred条件为false时，调用该函数才会阻塞调用线程
//且只有在收到其他线程通知后，只有当pred为true时，调用线程才会被解除阻塞


//condition_variable成员函数notify_one
//用于唤醒某个wait线程，如果当前没有wait线程，则什么也不做
//如果存在多个wait线程，唤醒其中一个，不可确定
//condition_variable成员函数notify_all
//唤醒所有wait线程，没有wait线程则什么都不做

//condition_variable_any类
//除了可以接受所有的lockble参数外，与只能接受unique_lock<mutex>的condition_variable一致

//cv_status::no_timeout 
//cv_status::timeout
//两个枚举类型用于在wait_for和wait_until函数返回时判断是否超时








int main() {

    example3();

    return 0;
}



