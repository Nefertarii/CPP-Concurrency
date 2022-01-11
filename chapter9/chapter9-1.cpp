#include "../headfile.h"

//c++17 新内容

/* 位于execution头文件中的执行策略可以传递到算法中
 * std::execution::seq/std::execution::par/std::execution::par_unseq
 * 这些设置控制着算法的行为:算法复杂度,抛出异常时的行为和执行的位置/方式/时间
 *
 * 算法复杂度
 * 向算法提供执行策略时,算法的复杂度就会发生变化:除了对并行的管理调度开销外,并行算法的核心操作将会被多次执行(交换/比较/以及提供的函数对象)
 * 目的是在总运行时间方面提供性能的整体改进
 *
 * 抛出异常时的行为
 * 具有执行策略的算法在执行期间触发异常,则结果执行策略确定
 * 如果有异常未被捕获,那么标准执行策略都会调用std::terminate
 * 如果标准库无法提供给内部操作足够的资源,则在无执行策略算法执行时,触发std::bad_alloc异常
 *
 * 算法执行的位置和时间
 * 这是执行策略的基本面,也是标准执行策略之间不同的地方
 * 相应执行策略指定使用那些代理来执行算法,无论这些代理是“普通”线程/向量流/GPU线程
 * 执行策略还将对算法步骤进行执行时的约束和安排:是否以特定的顺序运行,算法步骤之间是否可以交错,或彼此并行运行等
 */
 //std::execution::sequenced_policy 顺序执行
void func1() {
    std::vector<int> ivec(100);
    int count = 0;
    std::for_each(std::execution::seq, ivec.begin(), ivec.end(),
                  [&](int& x) {x = count++;});
}
//顺序策略并不是并行策略,这不仅需要在同一线程上执行所有操作,而且必须按照一定的顺序进行执行

//std::execution::parallel_policy 并行执行
//在给定线程上执行需要按照一定的顺序,不能交错执行,但十分具体的顺序是不指定的
void func2(std::vector<int>::iterator begin, std::vector<int>::iterator end) {
    std::for_each(std::execution::par, begin, end,
                  [](auto& x) {++x;});
    //大多数情况下都可以使用并行执行策略
    //但所需要的元素间有特定的顺序,或者对共享数据有非同步访问时,就会出现问题
    /*
    std::for_each(std::execution::par, v.begin(), v.end(),
                  [&](int& x) { x = ++count; });
    */
    //每次调用Lambda表达式时都会对计数器进行修改,如果有多个线程在执行Lambda表达式
    //那么这里就会出现数据竞争从而导致未定义行为
}

//std::execution::parallel_unsequenced_policy 并行不排序策略
//提供了最大程度的并行化算法,用以得到对算法使用的迭代器,相关值和可调用对象的严格要求
//使用该策略调用的算法,可以在任意线程上执行,这些线程彼此间没有顺序
//使用该策略时,算法使用的迭代器,相关值和可调用对象不能使用任何形式的同步,也不能调用任何需要同步的函数

/* 标准库中的大多数被执行策略重载的算法都在<algorithm>和<numeric>头文件中
 * 包括有:all_of, any_of, none_of, for_each, for_each_n, find, find_if, fin_end, 
 * find_first_of, adjacent_find, count, count_if, mismatch, equal, search, search_n,
 * copy, copy_n, copy_if, move, swap_ranges, transform, replace, replace_if, replace_copy,
 * replace_copy_if, fill, fill_n, generate, generate_n, remove, remove_if,
 * remove_copy, remove_copy_if, unique, unique_copy, reverse, reverse_copy,
 * rotate, rotate_copy, is_partitioned, partition, stable_partition, partition_copy,
 * sort, stable_sort, partial_sort, partial_sort_copy, is_sorted, is_sorted_until,
 * nth_element, merge, inplace_merge, includes, set_union, set_intersection, set_difference,
 * set_symmetric_difference, is_heap, is_heap_until, min_element, max_element, minmax_element,
 * lexicographical_compare, reduce, transform_reduce, exclusive_scan, inclusive_scan,
 * transform_exclusive_scan, transform_inclusive_scan, adjacent_difference
 * 对于列表中的每一个算法,每个”普通”算法的重载都有一个新的参数(第一个参数),这个参数将传入执行策略
 */
//有执行策略和没有执行策略的函数列表间有一个重要的区别,这只会影响到一部分算法
//如果“普通”算法允许输入迭代器或输出迭代器,那执行策略的重载则需要前向迭代器。
//因为输入迭代器是单向迭代的,只能访问当前元素,并且不能将迭代器存储到以前的元素
//输出迭代器只允许写入当前元素,不能在写入后面的元素后,后退再写入前面的元素
//(C++标准库定义了五类迭代器)
/* (输入迭代器):是用于检索值的单向迭代器,通常用于控制台或网络的输入,或生成序列
 * 该迭代器的任何副本都是无效的。
 * (输出迭代器):是用于向单向迭代器写入值,通常输出到文件或向容器添加值
 * 该迭代器会使该迭代器的任何副本失效。
 * (前向迭代器):是通过数据不变进行单向迭代的多路径迭代器
 * 虽然迭代器不能返回到前一个元素,但是可以存储前面元素的副本并使用它们引用
 * 前向迭代器返回对元素的实际引用，因此可以用于读写（如果目标是不是常量）
 * (双向迭代器):是像前向迭代器一样的多路径迭代器,但是它也可以后向访问之前的元素
 * (随机访问迭代器):是可以像双向迭代器一样前进和后退的多路径迭代器
 * 比单个元素大的跨距前进和后退,并且可以使用数组索引运算符,在偏移位置直接访问元素
 */
//这对于并行性很重要:意味着迭代器可以自由地复制,并等价地使用
//像增加正向迭代器不会使其他副本失效的特性,意味着单线程可以在迭代器的副本上操作,需要时增加副本,而不必担心使其他线程的迭代器失效

//具有内部同步并行算法的类可以使用std::execution::par
//无内部同步并行算法的类可使用std::execution::par_unseq

//一个并行记录网站日志的示例
struct log_info {
    std::string page;
    time_t visit_time;
    std::string browser;
    //...
};
extern log_info parse_log_line(std::string const& line) {
    //...
    return log_info();
}
using visit_map_type = std::unordered_map<std::string, unsigned long long>;
visit_map_type count_visit_page(std::vector<std::string> const& log_lines) {
    struct combine_visits {
        visit_map_type operator()(visit_map_type lhs, visit_map_type rhs) const {
            if (lhs.size() < rhs.size()) { std::swap(lhs, rhs); }
            for (auto const& entry : rhs) { lhs[entry.first] += entry.second; }
            return lhs;
        }
        visit_map_type operator()(log_info log, visit_map_type map) const {
            ++map[log.page];
            return map;
        }
        visit_map_type operator()(visit_map_type map, log_info log) const {
            ++map[log.page];
            return map;
        }
        visit_map_type operator()(log_info log1, log_info log2) const {
            visit_map_type map;
            ++map[log1.page];
            ++map[log2.page];
            return map;
        }
    };
    return std::transform_reduce(std::execution::par, log_lines.begin(), log_lines.end(),
                                 visit_map_type(), combine_visits(), parse_log_line);
}
//假设函数parse_log_line的功能是从日志条目中提取相关信息
//count_visits_per_page函数是一个简单的包装器,将对std::transform_reduce的调用进行包装
//复杂度来源于规约操作:需要组合两个log_info结构体来生成一个映射,一个log_info结构体和一个映射(无论是哪种方式),或两个映射
//std::transform_reduce将使用硬件并行执行此计算(因为传了std::execution::par)
//人工编写这个算法允许将实现并行性的艰苦工作委托给标准库实现者,这样开发者就可以专注于期望的结果了

int main() {
    ;
}
