#include "../headfile.h"

//线程处理前对数据进行划分
//快速排序有两个最基本的步骤,将数据划分到中枢元素之前或之后,然后对中枢元素之前和之后的两半数组再次进行快速排序
//这里不能通过对数据的简单划分达到并行,因为只有在一次排序结束后,才能知道哪些项在中枢元素之前和之后
//当要对这种算法进行并行化,很自然的会想到使用递归
//每一级的递归都会多次调用quick_sort函数,因为需要知道哪些元素在中枢元素之前和之后
//递归调用是完全独立的,因为其访问的是不同的数据集,并且每次迭代都能并发执行
//每层递归都产生一个新线程,最后就会产生大量的线程
//大量线程对性能有很大的影响,如果有太多的线程存在,那么应用将会运行的很慢
//使用std::async()可以为每一级生成小于数据块的异步任务
//而使用std::thread::hardware_concurrency()函数也能确定线程的数量
template <typename T>
struct sorter {
    struct chunk_to_sort {
        std::list<T> data;
        std::promise<std::list<T>> promise;
    };
    thread_safe_stack<chunk_to_sort> chunks;
    std::vector<std::thread> threads;
    //在栈上储存chunk 并且设置线程
    int const max_thread_count;
    std::atomic<bool> end_of_data;

    sorter() :max_thread_count(std::thread::hardware_concurrency() - 1),
        end_of_data(false) {}
    ~sorter() {
        end_of_data = true;
        //设置end_of_data 并等待其他线程完成操作
        for (int i = 0; i < threads.size(); i++) { threads[i].join(); }
        //完成后需要对线程进行清理
    }
    void try_sort_chunk() {
        std::shared_ptr<chunk_to_sort> chunk = chunks.pop();
        if (chunk) { sort_chunk(chunk); }
        //该函数从栈上弹出一个数据块饼进行排序
    }
    std::list<T> do_sort(std::list<T>& chunk_data) {
        if (chunk_data.empty()) { return chunk_data; }
        std::list<T> result;
        result.splice(result.begin(), chunk_data, chunk_data.begin());
        T const& partition_val = *result.begin();
        typename std::list<T>::iterator divide_point = std::partition(chunk_data.begin(), chunk_data.end(),
                                            [&](T const& val) { return val < partition_val;});
        chunk_to_sort new_lower_chunk;
        new_lower_chunk.data.splice(new_lower_chunk.data.end(),
                                    chunk_data, chunk_data.begin(),
                                    divide_point);
        //直至此处都是对数据进行划分
        std::future<std::list<T>> new_lower = new_lower_chunk.promise.get_future();
        chunks.push(std::move(new_lower_chunk));
        if (threads.size() < max_thread_count) {
            threads.push_back(std::thread(&sorter<T>::sort_thread, this));
        }
        //将数据推入栈中,并且如果有空余线程将使用空余线程
        std::list<T> new_higher(do_sort(chunk_data));
        result.splice(result.end(), new_higher);
        while (new_lower.wait_for(Sec(0)) != std::future_status::ready) {
            try_sort_chunk();
            //线程已经被使用了即开始尝试对数据进行处理
        }
        result.splice(result.begin(), new_lower.get());
        return result;
    }
    void sort_chunk(std::shared_ptr<chunk_to_sort> const& chunk) {
        chunk->promise.set_value(do_sort(chunk->data));
        //利用promise取回在其他线程中储存在栈上的数据
    }
    void sort_thread() {
        while (!end_of_data) {
            try_sort_chunk();
            std::this_thread::yield();
        }
        //循环检查end_of_data有没有被设置(主线程退出)
        //没有被设置则可以尝试获取栈上需要排序的数据块进行排序
        //同时把时间片交给其他线程使用,以确保其他线程也能进行操作
    }
};
template<typename T>
std::list<T> parallel_quick_sort(std::list<T> input) {
    if (input.empty()) { return input; }
    sorter<T> s;
    return s.do_sort(input);
    //排序的主线程,返回也在此处
    //所有线程的任务都来源于一个等待链表,然后线程会去完成任务
    //完成任务后会再来链表提取任务(这个线程池很有问题,包括竞争等等)
}

/*
 * 划分可以在处理前划分 也可以递归划分
 * 但当数据为动态长度时,这些将不起作用
 * 此时基于任务类型的划分,好于基于数据的划分
 * 任务类型划分工作,让线程做专门的工作,也就是每个线程做不同的工作
 * 线程可能会对同一段数据进行操作,但它们对数据进行不同的操作
 * 每个线程都有不同的任务意味着真正意义上的线程独立,其他线程偶尔会向特定线程交付数据,或是通过触发事件的方式来进行处理
 * 不过总体而言,每个线程只需要关注自己所要做的事情即可
 * 多线程下有两个危险需要分离关注,第一个是对错误担忧的分离,主要表现为线程间共享着很多的数据,或者不同的线程要相互等待
 * 这两种情况都是因为线程间很密切的交互,这种情况发生时,就需要看一下为什么需要这么多交互
 * 当所有交互都有同样的问题,就应该使用单线程来解决,并将引用同一源的线程提取出来
 * 或者当有两个线程需要频繁的交流,在没有其他线程时,就可以将这两个线程合为一个线程
 * 当通过任务类型对线程间的任务进行划分时,不应该让线程处于完全隔离的状态
 * 当多个输入数据集需要使用同样的操作序列,可以将序列中的操作分成多个阶段,来让每个线程执行
 * 当任务会应用到相同操作序列,去处理独立的数据项时,就可以使用流水线(pipeline)系统进行并发
 * 这好比一个物理管道:数据流从管道一端进入,进行一系列操作后,从管道另一端出去
 * 使用这种方式划分工作,可以为流水线中的每一阶段操作创建一个独立线程
 * 当一个操作完成,数据元素会放在队列中以供下一阶段的线程提取使用
 * 这就允许第一个线程在完成对于第一个数据块的操作并要对第二个数据块进行操作时,第二个线程可以对第一个数据块执行管线中的第二个操作
 */


int main() {
    std::list<int> list1 = { 5, 1, 6, 7, 2, 3, 6 };
    std::list<int> list2 = parallel_quick_sort(list1);
    for (auto i : list2) {
        std::cout << i << " ";
    }
}