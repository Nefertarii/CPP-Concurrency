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
        parallel_for_each2(mid_point, last, func);
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
            catch (...) {
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

//std::partial_sum
/* 会计算给定范围中的每个元素,并用计算后的结果将原始序列中的值替换掉
 * 如有一个序列[1, 2, 3, 4, 5],在执行该算法后会成为:[1, 3(1+2), 6(1+2+3), 10(1+...+4), 15(1+...+5)]
 * 对于多个处理器,可以对部分结果进行传播
 * 第一次与相邻的元素(距离为1)相加,之后和距离为2的元素相加,再后来和距离为4的元素相加,以此类推
 * 初始序列[1, 2, 3, 4,  5,  6,  7,  8,  9],下次隔1个元素
 * 第一次后[1, 3, 5, 7,  9,  11, 13, 15, 17],下次隔2个元素
 * 第二次后[1, 3, 6, 10, 14, 18, 22, 26, 30],下次隔4个元素
 * 第三次后[1, 3, 6, 10, 15, 21, 28, 36, 44],下次隔8个元素
 * 第四次后[1, 3, 6, 10, 15, 21, 28, 36, 45]为最终的结果
 * 这种方法提高了并行的可行性,使得每个处理器可在每一步中处理一个数据项
 */

//并行实现std::partial_sum
template <typename Iterator>
Iterator parallel_partal_sum(Iterator first, Iterator last) {
    typedef typename Iterator::value_type value_type;
    struct process_chunk {
        void operator()(Iterator begin, Iterator last, std::future<value_type>* previous_end_value,
                        std::promise<value_type>* end_value) {
            try {
                Iterator end = last;
                end++;
                std::partial_sum(begin, end, begin);
                //此处是对最后一个值进行处理
                if (previous_end_value) {
                    value_type addend = previous_end_value->get();
                    *last += addend;
                    if (end_value) { end_value->set_value(*last); }
                    std::for_each(begin, last, [addend](value_type& item) {item += addend;});
                    //当前块如果不是第一块,需要等待previous_end_value的值从前方的块传递过来
                    //为了算法的并行,将会首先对最后一个元素进行更新,以将其传递给下一个数据块
                    //完成后就可以用for_each对剩余的数据项进行更新
                }
                else if (end_value) { end_value->set_value(*last); }
                //previous_end_value的值为空,表明当前块为第一个数据块,因此只需为下一个数据块更新
            }
            catch (...) {
                if (end_value) { end_value->set_exception(std::current_exception()); }
                else { throw; }
                //在操作出现异常时,会将其捕获并存入承诺值
                //直到处理最后一个数据块时再抛出异常
            }
        }
    };
    long const length = std::distance(first, last);
    if (!length) return last;
    long const min_per_thread = 25;
    long const max_threads = (length + min_per_thread - 1) / min_per_thread;
    long const hardware_threads = std::thread::hardware_concurrency();
    long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    long const block_size = length / num_threads;
    typedef typename Iterator::value_type value_type;
    std::vector<std::thread> threads(num_threads - 1);
    std::vector<std::promise<value_type>> end_values(num_threads - 1);
    //线程和承诺指用于储存每块中的最后一个值
    std::vector<std::future<value_type>> previous_end_values;
    //期望值用于对前一块中的最后一个值进行检索
    previous_end_values.reserve(num_threads - 1);
    join_threads joiner(threads);
    Iterator block_start = first;
    for (int i = 0;i < (num_threads - 1);i++) {
        Iterator block_last = block_start;
        std::advance(block_last, block_size - 1);
        threads[i] = std::thread(process_chunk(), block_start, block_last,
                                 (i != 0) ? &previous_end_values[i - 1] : 0,
                                 &end_values[i]);
        block_start = block_last;
        ++block_start;
        previous_end_values.push_back(end_values[i].get_future());
    }
    Iterator final_element = block_start;
    std::advance(final_element, std::distance(block_start, last) - 1);
    process_chunk()(block_start, final_element,
                    (num_threads > 1) ? &previous_end_values.back() : 0, 0);
    //获取之前数据块中最后一个元素的迭代器,并将其作为参数传入process_chunk中
    return final_element;
}

//单个处理器处理对一定数量的元素执行同一条指令,这种方式称为单指令-多数据流(SIMD)
//因此代码必须能处理通用情况,并且需要在每步上对线程进行显式同步
//使用栅栏(barrier),一种同步机制:只有所有线程都到达栅栏处,才能进行之后的操作,先到达的线程必须等待未到达的线程



int main() {
    std::vector<int> ivec = { 1,2,3,4,5,6,7,8,9,10 };
    parallel_partal_sum(ivec.begin(), ivec.end());
    for (auto it : ivec) {
        std::cout << it << " ";
    }
}