#include "../headfile.h"

//并行算法的异常安全
//比起串行算法,并行算法更注重要求注意异常问题
//当一个操作在串行算法中抛出异常,只需考虑对其本身进行处理,以免资源泄露或破坏不变量
//同时允许将异常传递给调用者,由调用者来处理该异常
//在并行算法中,因为操作运行在独立的线程上,此时的异常不再允许被传播
//此时的异常会使调用堆栈出现问题,如果函数在创建新线程后带着异常退出将导致整个应用终止

template <typename Iterator, typename T>
struct accumulate_block {
    void operator()(Iterator first, Iterator last, T& result) {
        result = std::accumulate(first, last, result);
    }
};
template<typename Iterator, typename T>
T parallel_accmulate(Iterator first, Iterator last, T init) {
    long const length = std::distance(first, last);
    if (!length) { return init; }
    long const min_per_thread = 25;
    long const max_thread = (length + min_per_thread - 1) / min_per_thread;
    long const hardware_threads = std::thread::hardware_concurrency();
    long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_thread);
    long const block_size = length / num_threads;
    std::vector<T> results(num_threads);
    std::vector<std::thread> threads(num_threads - 1);
    //至此,调用线程没有做任何事情或产生新的线程,而构造threads抛出的异常会被析构函数清理
    Iterator block_start = first;
    for (long i = 0;i < (num_threads - 1);i++) {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        threads[i] = std::thread(accumulate_block<Iterator, T>(),
                                 block_start, block_end, std::ref(results[i]));
        //此处将创建第一个线程,如果在这里抛出异常将会使新创建的thread对象销毁
        //同时程序将调用std::terminate来中断整体的运行
        block_start = block_end;
    }
    accumulate_block<Iterator, T>()(block_start, last, results[num_threads - 1]);
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
    return std::accumulate(results.begin(), results.end(), init);
    //整体的代码是非异常安全的,很多地方的调用都可能抛出异常
    //像accumulate_block的调用处,函数的返回处
    //处理异常最好能确定所有抛出异常的地方,再来解决异常问题
    //若允许代码产生异常,可以与std::packaged_task和std::future相结合来解决
}

//使用std::packaged_task后的处理方式
template <typename Iterator, typename T>
struct accumulate_block2 {
    void operator()(Iterator first, Iterator last) {
        return std::accumulate(first, last, T());
        //结果直接返回不再储存 使用packaged_task和future是线程安全的,可以用来对结果进行转移
    }
};
class join_threads {
private:
    std::vector<std::thread>& threads;
public:
    explicit join_threads(std::vector<std::thread>& threads_) :threads(threads_) {}
    ~join_threads() {
        for (int i = 0; i < threads.size(); i++) {
            if (threads[i].joinable()) { threads[i].join(); }
        }
    }
};
template <typename Iterator, typename T>
T parallel_accmulate2(Iterator first, Iterator last, T init) {
    long const length = std::distance(first, last);
    if (!length) { return init; }
    long const min_per_thread = 25;
    long const max_threads = (length + min_per_thread - 1) / min_per_thread;
    long const hardware_thread = std::thread::hardware_concurrency();
    long const num_threads = std::min(hardware_thread != 0 ? hardware_thread : 2, max_threads);
    long const block_size = length / num_threads;
    std::vector<std::future<T>> futures(num_threads - 1);
    //通过使用期望值储存线程的结果
    std::vector<std::thread> threads(num_threads - 1);
    join_threads joiner(threads);
    //线程汇入主线程后将被自动删除
    Iterator block_start = first;
    for (unsigned long i = 0;i < (num_threads - 1);i++) {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        std::packaged_task<T(Iterator, Iterator)> task(accumulate_block2<Iterator, T>());
        futures[i] = task.get_future();
        threads[i] = std::thread(std::move(task), block_start, block_end);
        block_start = block_end;
        //在此处创建任务并从任务中获取期望值,再将需要处理的数据块的开始与结束信息传入线程
        //任务执行时期望值会获取对应的结果或抛出异常
    }
    T last_result = accumulate_block2<Iterator, T>()(block_start, last);
    //期望值不能获得一组结果数组,所以需要将最终数据块的结果赋给一个变量进行保存
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
    T result = init;
    for (long i = 0; i < (num_threads - 1); i++) { result += futures[i].get(); }
    //使用简for循环在这里比使用std::accumulate好,循环从提供的初始值开始,并且将每个期望值上的值进行累加
    //对future.get的调用会阻塞线程,直到结果准备就绪
    //如果相关任务抛出一个异常,就会被期望值捕捉到,并且使用get()的时候获取数据时,这个异常会再次抛出
    //当生成第一个新线程和当所有线程都汇入主线程时抛出异常,会让线程产生泄露
    result += last_result;
    return result;
}

//使用std::async的处理方式
template <typename Iterator, typename T>
T parallel_accmulate3(Iterator first, Iterator last, T init) {
    long const length = std::distance(first, last);
    long const max_chunk_size = 25;
    if (length <= max_chunk_size) { return std::accumulate(first, last, init); }
    //数据长度小于最大限制可以直接调用accumulate
    else {
        Iterator mid_point = first;
        std::advance(mid_point, length / 2);
        std::future<T> first_half_result = std::async(parallel_accmulate3<Iterator, T>,
                                                      first, mid_point, init);
        T second_half_result = parallel_accmulate3(mid_point, last, T());
        //通过递归将数据分成两部分,再生成一个异步任务对另外一半数据进行处理
        //async能够保证充分利用硬件线程,也不会产生超额认购,同时也是异常安全的
        //通过调用async产生的期望值,将会在异常传播时被销毁
        //另外当异步任务抛出异常且被期望值所捕获,会在调用get()时抛出
        return first_half_result.get() + second_half_result;
    }
}

//可扩展性
/* 对于任意的多线程程序,运行时的工作线程数量会有所不同
 * 应用初始阶段只有一个线程,之后会在这个线程上衍生出新的线程
 * 理想状态:每个线程都做着有用的工作,不过这种情况几乎是不可能发生的,线程通常会花时间进行互相等待,或等待IO操作的完成。
 * 一种简化的方式就是就是将程序划分成“串行”部分和“并行”部分
 * 串行部分是由单线程执行一些工作的地方
 * 并行部分是可以让所有可用的处理器一起工作的部分
 * 当在多处理系统上运行应用时,并行部分理论上会完成的相当快,因为其工作被划分为多份,放在不同的处理器上执行
 * 串行部分则不同,只能一个处理器执行所有工作,这样的(简化)假设下,就可以随着处理数量的增加而增加性能
 */
//Amdahl定律
/* 代表了处理器并行运算之后效率提升的能力
 * Time = s + f/p
 * s为串行部分 f为并行部分 p为核心数
 * 对代码最大化并发可以保证所有处理器都能用来做有用的工作,
 * 如果将串行部分的减小,或者减少线程的等待,就可以在多处理器的系统中获取更多的性能收益。
 * 当能提供更多的数据让系统进行处理,并且让并行部分做最重要的工作,就可以减少串行部分,也能获取更高的性能增益
 * 当有更多的处理器加入时,是减少一个动作的执行时间,或在给定时间内做更多工作,辨别这两个方面哪个能扩展十分重要
 */

int main() {
    
}