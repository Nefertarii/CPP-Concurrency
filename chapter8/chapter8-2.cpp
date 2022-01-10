#include "../headfile.h"

//线程的中断和启动

/*class interruptible_thread {
public:
    template <typename Function>
    interruptible_thread(Function func);
    void join();
    void detach();
    bool joinable() const;
    void interrupt();
};*/
class thread_interrupted {
private:
    std::string except;
public:
    thread_interrupted(std::string_view str = "Thread interrupt") :except(str) {}
    //异常构造函数传入的类型为string_view
    std::string what() const { return except; }
};

//在不添加多余的数据的前提下,为了使断点能够正常使用,就需要使用一个没有参数的函数
//这意味着中断数据结构可以访问thread_local变量,并在线程运行时,对变量进行设置
class interrupt_flag {
private:
    std::atomic<bool> flag;
public:
    void set() { flag.store(true, std::memory_order_relaxed); }
    bool is_set() const { return flag.load(std::memory_order_relaxed); }
};
thread_local interrupt_flag this_thread_interrupt_flag;
class interruptible_thread {
private:
    std::thread internal_thread;
    interrupt_flag* flag;
public:
    template<typename Function>
    interruptible_thread(Function func) {
        std::promise<interrupt_flag*> p;
        internal_thread = std::thread([func, &p] {p.set_value(&this_thread_interrupt_flag);
        func(); });
        flag = p.get_future().get();
    }
    //线程持有func的副本和本地承诺值变量的引用,lambda函数设置承诺值到flag的地址中
    //即使lambda函数在新线程上执行,对本地变量p进行悬空引用也不会出现问题
    //因为在新线程返回前,类的构造函数会等待变量p不被引用再使用
    //该实现没有考虑汇入/分离线程,所以在flag变量在线程退出/分离前声明,来避免悬空
    void interrupt() { if (flag) { flag->set(); } }
};
void interruption_point() {
    if (this_thread_interrupt_flag.is_set()) {
        throw thread_interrupted();
    }
    //通过检查线程是否被中断,并抛出一个thread_interrupted异常
}
//当线程阻塞时,显式的调用函数进行中断就会没有意义
/*
void interruptible_wait(std::condition_variable& cv, std::unique_lock<std::mutex>& lk) {
    interruption_point();
    this_thread_interrupt_flag.set_condition_variable(cv);
    cv.wait(lk);
    this_thread_interrupt_flag.clear_condition_variable();
    interruption_point();
}
*/
//函数通过interrupt_flag对当前线程关联-等待-清除条件变量
//如果线程在等待期间被条件变量中断,中断线程将广播条件变量,并唤醒等待该条件变量的线程,以此来检查中断
//该段代码仍不完善,在面临抛出异常和条件竞争的问题上无法正确处理


class interrupt_flag2 {
private:
    std::atomic<bool> flag;
    std::condition_variable* thread_cond;
    std::mutex set_clear_mtx;
public:
    interrupt_flag2() :thread_cond(0) {}
    void set() {
        flag.store(true, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(set_clear_mtx);
        if (thread_cond) { thread_cond->notify_all(); }
    }
    bool is_set() const { return flag.load(std::memory_order_relaxed); }
    void set_condition_variable(std::condition_variable& cv) {
        std::lock_guard<std::mutex> lk(set_clear_mtx);
        thread_cond = &cv;
    }
    void clear_condition_variable() {
        std::lock_guard<std::mutex> lk(set_clear_mtx);
        thread_cond = 0;
    }
    struct clear_cv {
        ~clear_cv();
    };
};
thread_local interrupt_flag2 this_thread_interrupt_flag2;
interrupt_flag2::clear_cv::~clear_cv() {
    this_thread_interrupt_flag2.clear_condition_variable();
};

void interruptible_wait2(std::condition_variable& cv, std::unique_lock<std::mutex>& lk) {
    interruption_point();
    this_thread_interrupt_flag2.set_condition_variable(cv);
    interrupt_flag2::clear_cv guard;
    interruption_point();
    cv.wait_for(lk, Ms(1));
    interruption_point();
}
template<typename Predicate>
void interruptible_wait2(std::condition_variable& cv,
                         std::unique_lock<std::mutex>& lk, Predicate pred) {
    interruption_point();
    this_thread_interrupt_flag2.set_condition_variable(cv);
    interrupt_flag2::clear_cv guard;
    while (!this_thread_interrupt_flag2.is_set() && !pred()) {
        cv.wait_for(lk, Ms(1));
    }
    interruption_point();
    //通过设置超时变量进行等待
}


//通过使用std::condition_variable_any能比std::condition_variable更加灵活
//any能够使用任意类型的锁,做得更方便,简单
//通过设计自己的锁,上锁/解锁interrupt_flag的内部互斥量set_clear_mtx
class interrupt_flag3 {
private:
    std::thread internal_thread;
    std::atomic<bool> flag;
    std::condition_variable* thread_cond;
    std::condition_variable_any* thread_cond_any;
    std::mutex set_clear_mtx;
public:
    interrupt_flag3() :thread_cond(0), thread_cond_any(0) {}
    void set() {
        flag.store(true, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(set_clear_mtx);
        if (thread_cond) { thread_cond->notify_all(); }
        else if (thread_cond_any) { thread_cond_any->notify_all(); }
    }
    template <typename Lockble>
    void wait(std::condition_variable_any& cv, Lockble& lk) {
        struct custom_lock {
            interrupt_flag3* self;
            Lockble& lk;
            custom_lock(interrupt_flag3* self_, std::condition_variable_any& cond,
                        Lockble& lk_) : self(self_), lk(lk_) {
                self->set_clear_mtx.lock();
                self->thread_cond_any = &cond;
                //在构造时锁住内部的锁防止竞争,再引用cond_any传入内部
            }
            void unlock() {
                lk.unlock();
                self->set_clear_mtx.unlock();
                //当条件变量调用的wait时自定义锁unlock将进行解锁
                //对lk和set_cleat_mtx进行解锁后能让线程尝试中断其他线程获取set_clear_mtx锁
            }
            void lock() { std::lock(self->set_clear_mtx, lk); }
            //wait结束等待后,线程会自动调用lock函数,这将锁住lk
            ~custom_lock() {
                self->thread_cond_any = 0;
                self->set_clear_mtx.unlock();
            }
            //清理cond_any指针同样会解锁set_clear_mtx,以此可对中断变量再次进行检查
        };
        custom_lock custom_lk(this, cv, lk);
        interrupt_flag3();
        cv.wait(custom_lk);
        interruption_point();
    }
};
thread_local interrupt_flag3 this_thread_interrupt_flag3;
class interruptible_thread2 {
private:
    std::thread internal_thread;
    interrupt_flag3* flag;
public:
    template<typename Function>
    interruptible_thread2(Function func) {
        std::promise<interrupt_flag3*> p;
        //因为thread_interrupted是一个异常,是为了确保资源不会泄露,在数据结构中留下对应的退出状态
        //但异常传入std::thread的析构函数时,将会调用std::terminate(),并且整个程序将会终止
        //为了避免这种情况,需要在每个将interruptible_thread变量作为参数传入的函数中放置catch(thread_interrupted)处理块
        internal_thread = std::thread([func, &p] {
            p.set_value(&this_thread_interrupt_flag3);
            try { func(); }
            catch (thread_interrupted const&) {}
                                      });
    }
    void interrupt() { if (flag) { flag->set(); } }
};
template <typename Lockable>
void interruptible_wait(std::condition_variable_any& cv, Lockable& lk) {
    this_thread_interrupt_flag3.wait(cv, lk);
    //wait结束等待会自动调用lock()
}
//后台进程需要一直执行,直到应用退出
//后台线程会作为应用启动的一部分被启动,并且在应用终止的时候停止运行
//通常这样的应用只有在机器关闭时才会退出,因为应用需要更新应用最新的状态,就需要全时间运行
//在某些情况下,当应用被关闭,需要使用有序的方式将后台线程关闭,其中一种方式就是中断
//但线程被中断后不会马上结束,因为需要对下一个断点进行处理,并且在退出前执行析构函数和代码异常处理部分
//因为需要汇聚每个线程,所以就会让中断线程等待,即使线程还在做着有用的工作(如中断其他线程)
//只有当没有工作时(所有线程都被中断)不需要等待
//这就允许中断线程并行的处理自己的中断,并更快的完成中断


/*
//在桌面上查找一个应用,这就需要与用户互动,应用的状态需要能在显示器上显示,才能看出应用有什么改变
//简单的后台监视文件系统
std::mutex config_mutex;
std::vector<interruptible_thread> background_threads;
void background_thread(int disk_id) {
    while (true)   {
        interruption_point();
        fs_change fsc = get_fs_changes(disk_id);
        if (fsc.has_changes())     {
            update_index(fsc);
        }
    }
}
void start_background_processing() {
    background_threads.push_back(
        interruptible_thread(background_thread, disk_1));
    background_threads.push_back(
        interruptible_thread(background_thread, disk_2));
}
int main() {
    start_background_processing();
    process_gui_until_exit();
    std::unique_lock<std::mutex> lk(config_mutex);
    for (unsigned i = 0;i < background_threads.size();++i)   {
        background_threads[i].interrupt();
    }
    for (unsigned i = 0;i < background_threads.size();++i)   {
        background_threads[i].join();
    }
}
//启动时,后台线程就已经启动,之后,对应线程将会处理GUI
//用户要求进程退出时,后台进程将会被中断,并且主线程会等待每一个后台线程结束后才退出
//后台线程运行在一个循环中,并时刻检查磁盘的变化,对其序号进行更新
//调用interruption_point()函数可以在循环中对中断进行检查
*/

int main() {
    ;
}