#include "../headfile.h"

//线程池
//线程池可用的线程数量,任务的分配方式,以及等待的方式是需要关注的重点

//一个简单的线程池
//拥有固定数量的工作线程,工作需要完成时,可以调用函数将任务挂在任务队列中
//每个工作线程都会从任务队列上获取任务,然后执行这个任务,执行完成后再回来获取新的任务
//线程就不需要等待其他线程完成对应任务了,如果需要等待,就需要对同步进行管理

class thread_pool {
private:
    std::atomic_bool done;
    thread_safe_queue<std::function<void()>> work_queue;
    std::vector<std::thread> threads;
    //工作用的线程组
    join_threads joiner;
    void worker_thread() {
        while (!done) {
            std::function<void()> task;
            if (work_queue.try_pop(task)) { task(); }
            else { std::this_thread::yield(); }
        }
        //从任务队列中获取并执行任务,没有则休眠线程
    }
public:
    thread_pool() :done(fasle), joiner(threads) {
        int const thread_const = std::thread::hardware_concurrency();
        try {
            for (int i = 0;i < thread_const;i++) {
                threads.push_back(std::thread(&thread_pool::worker_thread, this));
            }
        }
        catch (...) {
            done = true;
            throw;
        }
    }
    template <typenmae Function>
    void submit(Function func) { work_queue.push(std::function<void()>(func)); }
    //通过std::function<void()>对任务进行封装, 并将其推入队列中
    ~thread_pool() { done = true; }
    //这里的线程池只适用于没有返回,没有阻塞的简单任务
}

//更复杂的线程池