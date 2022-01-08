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
    void run_pending_task();
};
//该线程池在任务中有依赖关系将无法工作

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

//通过在thread_pool中添加一个新函数,来执行任务队列上的任务,并对线程池进行管理
//高级线程池的实现可能会在等待函数中添加逻辑,或等待其他函数来处理这个任务,优先的任务会让其他的任务进行等待
void thread_pool2::run_pending_task() {
    function_wrapper task;
    if (work_queue.try_pop(task)) { task(); }
    else { std::this_thread::yield(); }
}
//去掉了在worker_thread()函数的主循环,函数任务队列中有任务的时候执行任务,没有就会让操作系统对线程进行重新分配

/*
template<typename T>
struct sorter {
    thread_pool2 pool;
    std::list<T> do_sort(std::list<T>& chunk_data) {
        if (chunk_data.empty()) { return chunk_data; }
        std::list<T> result;
        result.splice(result.begin(), chunk_data, chunk_data.begin());
        T const& partition_val = *result.begin();
        typename std::list<T>::iterator divide_point =
            std::partition(chunk_data.begin(), chunk_data.end(),
                           [&](T const& val) {return val < partition_val;});
        std::list<T> new_lower_chunk;
        new_lower_chunk.splice(new_lower_chunk.end(), chunk_data,
                               chunk_data.begin(), divide_point);
        std::future<std::list<T>> new_lower =
            pool.submit(std::bind(&sorter::do_sort, this, std::move(new_lower_chunk)));
        //线程每次调用线程池的submit()函数,都会推送一个任务到工作队列中
        //这意味着随着处理器的增加,任务队列上就会有很多的竞争,这会让性能下降
        //使用无锁队列会让任务没有明显的等待,但乒乓缓存会消耗大量的时间
        std::list<T> new_higher(do_sort(chunk_data));
        result.splice(result.end(), new_higher);
        while (!(new_lower.wait_for(Sec(0)) == std::future_status::timeout)) {
            pool.run_pending_task();
        }
        result.splice(result.begin(), new_lower.get());
        return result;
    }
};
*/
//为了避免乒乓缓存,每个线程建立独立的任务队列
//这样每个线程就会将新任务放在自己的任务队列上,并且当线程上的任务队列没有任务时,会去全局的任务列表中取任务


class thread_pool3 {
private:
    std::atomic_bool done;
    thread_safe_queue<function_wrapper> pool_work_queue;
    typedef std::queue<function_wrapper> local_queue_type;
    static thread_local std::unique_ptr<local_queue_type> local_work_queue;
    //使用std::unique_ptr指向线程本地的工作队列,这个指针在worker_thread()中进行初始化
    //std:unique_ptr的析构函数会保证在线程退出的时候工作队列被销毁
    void worker_thread() {
        local_work_queue.reset(new local_queue_type);
        while (!done) {
            run_pending_task();
        }
    }
public:
    template<typename Function>
    std::future<typename std::result_of<Function()>::type> submit(Function func) {
        typedef typename std::result_of<Function()>::type result_type;
        std::packaged_task<result_type()> task(func);
        std::future<result_type> res(task.get_future());
        if (local_work_queue) { local_work_queue->push(std::move(task)); }
        else { pool_work_queue.push(std::move(task)); }
        return res;
    }
    //submit会检查当前线程是否具有一个工作队列,如果有就可以将任务放入线程的本地队列
    //否在放入线程池的全局队列中
    void run_pending_task() {
        function_wrapper task;
        if (local_work_queue && !local_work_queue->empty()) {
            task = std::move(local_work_queue->front());
            local_work_queue->pop();
            task();
        }
        else if (pool_work_queue.try_pop(task)) { task(); }
        else { std::this_thread::yield(); }
    }
    //run_pending_task同样对本地队列进行检查,如果队列中有任务则会从第一个开始执行
    //如果没有则从全局工作列表上获取
};
//这样的线程池会在任务分配不均匀时导致有些线程非常多任务,而有些线程则闲置
//可以通过窃取任务的方式,让没有工作的线程从其他线程的任务队列中获取任务


class work_steal_queue {
private:
    typedef function_wrapper data_type;
    std::deque<data_type> queue;
    mutable std::mutex mtx;
public:
    work_steal_queue() {}
    work_steal_queue(const work_steal_queue& other) = delete;
    work_steal_queue& operator=(const work_steal_queue& other) = delete;
    void push(data_type data) {
        std::lock_guard<std::mutex> lk(mtx);
        queue.push_front(std::move(data));
    }
    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx);
        return queue.empty();
    }
    bool try_pop(data_type& res) {
        std::lock_guard<std::mutex> lk(mtx);
        if (queue.empty()) { return false; }
        res = std::move(queue.front());
        queue.pop_front();
        return true;
    }
    bool try_steal(data_type& res) {
        std::lock_guard<std::mutex> lk(mtx);
        if (queue.empty()) { return false; }
        res = std::move(queue.back());
        queue.pop_back();
        return true;
    }
    //对std::deque<fuction_wrapper>进行了简单的包装,通过一个互斥锁来对所有访问进行控制
    //push和try_pop对队列的前端进行操作,try_steal对队列的后端进行操作
};
//拥有任务窃取的线程池
class thread_pool4 {
private:
    typedef function_wrapper task_type;
    std::atomic_bool done;
    thread_safe_queue<task_type> pool_work_queue;
    std::vector<std::unique_ptr<work_steal_queue>> queues;
    std::vector<std::thread> threads;
    join_threads joiner;
    static thread_local work_steal_queue* local_work_queue;
    static thread_local int index;
    void worker_thread(int index_) {
        index = index_;
        local_work_queue = queues[index].get();
        while (!done) { run_pending_task(); }
    }
    bool pop_task_from_local_queue(task_type& task) {
        return local_work_queue && local_work_queue->try_pop(task);
    }
    bool pop_task_from_pool_queue(task_type& task) {
        return pool_work_queue.try_pop(task);
    }
    bool pop_task_from_other_thread_queue(task_type& task) {
        for (int i = 0;i < queues.size();i++) {
            int const new_index = (index + i + 1) % queues.size();
            if (queues[new_index]->try_steal(task)) { return true; }
        }
        return false;
    }
public:
    thread_pool4() :done(false), joiner(threads) {
        int const thread_count = std::thread::hardware_concurrency();
        try {
            for (int i = 0;i < thread_count;i++) {
                queues.push_back(std::unique_ptr<work_steal_queue>(new work_steal_queue));
                threads.push_back(std::thread(&thread_pool4::worker_thread, this, i));
            }
        }
        catch (...) {
            done = true;
            throw;
        }
    }
    template <typename Function>
    std::future<typename std::result_of<Function()>::type> submit(Function func) {
        typedef typename std::result_of<Function()>::type result_type;
        std::packaged_task<result_type()> task(func);
        std::future<result_type> res(task.get_future());
        if (local_work_queue) { local_work_queue->push(std::move(task)); }
        else { pool_work_queue.push(std::move(task)); }
        return res;
    }
    void run_pending_task() {
        task_type task;
        if (pop_task_from_local_queue(task) ||
            pop_task_from_pool_queue(task) ||
            pop_task_from_other_thread_queue(task)) {
            task();
        }
        else { std::this_thread::yield(); }
    }
    ~thread_pool4() { done = true; }
};


int main() {

}
