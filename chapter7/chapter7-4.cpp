#include "../headfile.h"

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

//并行实现std::for_each
template<typename Iterator, typename Func>
void parallel_for_each(Iterator first, Iterator last, Func func) {
    long const length = std::distance(first, last);
    if (!length) { return; }
    long const min_per_thread = 25;
    long const max_threads = (length + min_per_thread - 1) / min_per_thread;
    long const hardware_threads = std::thread::hardware_concurrency();
    long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    long const block_size = length / num_threads;
    std::vector<std::future<void>> futures(num_threads - 1);
    std::vector<std::thread> threads(num_threads - 1);
    join_threads joiner(threads);

    Iterator block_start = first;
    for (int i = 0;i < (num_threads - 1);i++) {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        std::packaged_task<void(void)> task([=]() {std::for_each(block_start, block_end, func);});
        //使用lambda函数使范围内的任务执行f函数
        futures[i] = task.get_future();
        threads[i] = std::thread(std::move(task));
        block_start = block_end;
    }
    std::for_each(block_start, last, func);
    for (int i = 0;i < (num_threads - 1);i++) { futures[i].get(); }
    //该步骤只是为了决定是否将出现的异常进行传递
}
//async版本
template<typename Iterator, typename Func>
void parallel_for_each2(Iterator first, Iterator last, Func func) {
    long const length = std::distance(first, last);
    if (!length) { return; }
    long const min_per_thread = 25;
    if (length < (2 * min_per_thread)) { std::for_each(first, last, func); }
    else {
        Iterator const mid_point = first + (length / 2);
        std::future<void> first_half = std::async(&parallel_for_each2<Iterator, Func>,
                                                  first, mid_point, func);
        parallel_for_each2(mid_point, last, f);
        //同样将数据分为两部分,异步执行另外一部分
        first_half.get();
    }
}

//并行实现std::find
//find算法不同于上一个for_each,当元素满足中查找标准时,算法就可以直接退出而无需对其他元素进行搜索了
template<typename Iterator, typename MatchT>
Iterator parallel_find(Iterator first, Iterator last, MatchT match) {
    struct find_element {
        void operator()(Iterator begin, Iterator end, MatchT match,
                        std::promise<Iterator>* result,
                        std::atomic<bool>* done_flag) {
            try {
                for (;(begin != end) && !done_flag->load();begin++) {
                    if (*begin == match) {
                        result->set_value(begin);
                        done_flag->store(true);
                        return;
                    }
                }
            }
            catch(...) {
                try {
                    result->set_exception(std::current_exception());
                    done_flag->store(true);
                }
                catch (...) {}
                //此处用于捕获查找中抛出的异常
                //在承诺值储存前对完成标识进行设置同时捕获异常并抛弃
            }
        }
        //该结构体用于完成查找工作
        //循环检查给定数据块中的元素和完成标识
        //如果匹配的元素被找到,将结果设置到承诺值中,并设置标识然后返回
    };
    long const length = std::distance(first, last);
    if (!length) { return last; }
    long const min_per_thread = 25;
    long const max_threads = (length + min_per_thread - 1) / min_per_thread;
    long const hardware_threads = std::thread::hardware_concurrency();
    long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    long const block_size = length / num_threads;

    std::promise<Iterator> result;
    std::atomic<bool> done_flag(false);
    //用于停止搜索的两个变量
    std::vector<std::thread> threads(num_threads - 1);
    {
        join_threads joiner(threads);
        Iterator block_start = first;
        for (int i = 0;i < (num_threads - 1);i++) {
            Iterator block_end = block_start;
            std::advance(block_end, block_size);
            threads[i] = std::thread(find_element(), block_start, block_end,
                                     match, &result, &done_flag);
            block_start = block_end;
        }
        find_element()(block_start, last, match, &result, &done_flag);
        //新线程在查找的过程中,主线程同时也在对剩下的元素进行查找
    }
    if (!done_flag.load()) { return last; }
    //同时由于在启动-汇总在上方一个代码块中,所有线程都会在找到匹配元素时进行汇入
    return result.get_future().get();
    //获取查找返回或是异常
}
template<typename Iterator, typename MatchT>
//async版本
Iterator parallel_find2_impl(Iterator first, Iterator last, MatchT match, std::atomic<bool>& done) {
    try {
        long const length = std::distance(first, last);
        long const min_per_thread = 25;
        if (length < (2 * min_per_thread)) {
            for (;(first != last) && !done.load();first++) {
                if (*first == match) {
                    done = true;
                    return first;
                }
            }
            return last;
        }
        else {
            Iterator const mid_point = first + (length / 2);
            std::future<Iterator> async_result =
                std::async(&parallel_find2_impl<Iterator, MatchT>, mid_point,
                           last, match, std::ref(done));
            Iterator const direct_result = parallel_find2_impl(first, mid_point, match, done);
            return (direct_result == mid_point) ? async_result.get() : direct_result;
            //同样将数据分成两部分,通过不同线程来分别执行
        }
    }
    //函数无论是因为已经查找到最后一个,还是因为其他线程对done进行了设置,都会停止查找
    //如果没有找到,会将最后一个元素last进行返回
        catch (...) {
        done = true;
        throw;
    }
}
template <typename Iterator, typename MatchT>
Iterator parallel_find2(Iterator first, Iterator last, MatchT match) {
    std::atomic<bool>* done(false);
    return parallel_find2(first, last, match, done);
}

//并行实现std::partial_sum