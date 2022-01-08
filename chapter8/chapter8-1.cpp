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
    thread_pool() :done(false), joiner(threads) {
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
    template <typename Function>
    void submit(Function func) { work_queue.push(std::function<void()>(func)); }
    //通过std::function<void()>对任务进行封装, 并将其推入队列中
    ~thread_pool() { done = true; }
    //这里的线程池只适用于没有返回,没有阻塞的简单任务
};

//更复杂的线程池
//使用submit()返回对任务描述的句柄,等待任务完成,任务句柄用条件变量或期望值包装
//有些任务需要子线程返回一个结果到主线程上进行处理,在线程工作完成后会返回一个结果到等待线程中去
//std::packaged_task<>是不可拷贝的,但std::function()是需要储存可赋值构造的函数对象
class function_wrapper {
private:
    struct impl_base {
        virtual void call() = 0;
        virtual ~impl_base() {}
    };
    std::unique_ptr<impl_base> impl;
    template<typename Func>
    struct impl_type :impl_base {
        Func func;
        impl_type(Func&& func_) :func(std::move(func_)) {}
        void call() { func(); };
    };
public:
    template<typename Func>
    function_wrapper(Func&& func) :impl(new impl_type<Func>(std::move(func))) {}
    void operator()() { impl->call(); }
    function_wrapper() = default;
    function_wrapper(function_wrapper&& other) :impl(std::move(other.impl)) {}
    function_wrapper& operator=(function_wrapper&& other) {
        impl = std::move(other.impl);
        return *this;
    }
    function_wrapper(const function_wrapper&) = delete;
    function_wrapper(function_wrapper&) = delete;
    function_wrapper& operator=(const function_wrapper&) = delete;
};
class thread_pool2 {
private:
    std::atomic_bool done;
    thread_safe_queue<function_wrapper> work_queue;
    void work_thread() {
        while (!done) {
            function_wrapper task;
            if (work_queue.try_pop(task)) { task(); }
            else { std::this_thread::yield(); }
        }
    }
public:
    template<typename Function>
    std::future<typename std::result_of<Function()>::type> submit(Function func) {
        typedef typename std::result_of<Function()>::type result_type;
        //result_of用于在编译的时候推导出一个可调用对象的返回值类型
        std::packaged_task<result_type()> task(std::move(func));
        std::future<result_type> res(task.get_future());
        //func被包装入packaged_task,因为func是无参数的函数或可调用对象,所以能够使用result_of返回类型
        //在向队列推送任务和返回期望值之前,可以从task中获取期望值
        //当任务推送后,因为packaged_task是不可拷贝的,只能使用std::move获取
        return res;
        //返回一个std::future<>保存任务的返回值,并且可以等待调用者等待任务结束
    }
};//该线程池在任务中有依赖关系将无法工作

template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init) {
    long const length = std::distance(first, last);
    if (!length) { return init; }
    long const block_size = 25;
    long const num_blocks = (length + block_size - 1) / block_size;
    //这里通过使用的块数而不是线程数量,能利用线程池的最大化可扩展性
    //将工作块划分为最小工作块,当线程池里线程不多时,每个线程将处理多个工作块
    //硬件线程越多,可并发执行的工作块也就越多
    std::vector<std::future<T>> futures(num_blocks - 1);
    thread_pool pool;
    Iterator block_start = first;
    for (int i = 0;i < (num_blocks - 1);i++) {
    //i++调用的是迭代器i的T operator++(int)重载
    //而该函数需要先返回操作前的值,所以会先用一个临时变量保存i,然后再++操作,最后以值的方式返回这个临时变量
    //而++i调用的是迭代器i的T& operator++()重载
    //这个函数会直接进行++操作后返回自身的引用
    //后置++有时会导致不必要的复制,内存分配等操作
    //现代编译器O2优化已经能很好的处理这种问题,所以不必过于在意这个问题
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        futures[i] = pool.submit(accumulate_block<Iterator, T>());
        block_start = block_end;
    }
    T last_result = accumulate_block<Iterator, T>()(block_start, last);
    T result = init;
    for (int i = 0;i < (num_blocks - 1);i++) { result += futures[i].get(); }
    result += last_result;
    return result;
    //通过使用线程池的parallel_accumuate,线程池会通过submit()将异常返回给期望值
    //并在获取期望值时抛出,如果函数异常退出,线程池的析构函数会丢弃未完成的任务,等待工作中的任务完成
}

int main() {
    ;
}
