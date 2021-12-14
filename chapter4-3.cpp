#include "headfile.h"

//释放操作序列(内存模型)
//栅栏

std::vector<int> queue_data;
std::atomic<int> count;
void populate_queue() {
    int const items = 20;
    queue_data.clear();
    for (int i = 0;i < items;i++) {
        queue_data.push_back(i);
    }
    count.store(items, std::memory_order_release);
}
void populate_queue2() {
    sleep(2);
    int const items = 20;
    queue_data.clear();
    for (int i = 0;i < items;i++) {
        queue_data.push_back(i);
    }
    count.store(items, std::memory_order_release);
}
void consume_queue_items() {
    int item_index;
    while (true) {
        item_index = count.fetch_sub(1, std::memory_order_acquire);
        if (item_index <= 0) {
            continue;
        }
        printf("%d ", queue_data[item_index - 1]);
    }
}
void func1() {
    std::thread T1(populate_queue);
    std::thread T2(consume_queue_items);
    std::thread T3(consume_queue_items);
    std::thread T4(populate_queue2);
    T1.join();
    T2.join();
    T3.join();
    T4.join();
    //线程T1产生数据,并存储到一个共享缓存中,而后调用count.store()让其他线程知道数据是可用的
    //线程群会消耗队列中的元素,之后可能调用count.fetch_sub()向队列索取一个元素
    //在这之前需要对共享缓存进行完整的读取,一旦count归零,没有元素,线程必须等待
    //当只有一个消费者线程时,fetch_sub()是一个带有memory_order_acquire的读取操作
    //并且存储操作是带有memory_order_release语义,所以存储与加载同步,线程可以从缓存中读取元素
    //当有两个读取线程时,第二个fetch_sub()操作将看到被第一个线程修改的值,且没有值通过store写入其中
    //第二个线程与第一个线程不存在先行关系,并且对共享缓存中值的读取也不安全,
    //除非第一个fetch_sub()是带有memory_order_release语义的
    //这个语义为两个消费者线程建立了不必要的同步,无论是释放序列的规则,还是带有memory_order_release语义的fetch_sub操作
    //第二个消费者看到的是一个空的queue_data,无法从其获取任何数据,并且还会产生条件竞争
    //幸运的是第一个fetch_sub()对释放顺序做了一些事情,所以store()能同步与第二个fetch_sub()操作,两个消费者线程间不需要同步关系
}

std::atomic<bool> atomic_x, atomic_y;
std::atomic<int> atomic_int;
void fence_write_x_y() {
    atomic_x.store(true, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    atomic_y.store(true, std::memory_order_relaxed);
}
void fence_read_y_x() {
    while (!atomic_y.load(std::memory_order_relaxed)) { ; }
    std::atomic_thread_fence(std::memory_order_acquire);
    //栅栏操作会对内存序列进行约束,使其无法对任何数据进行修改
    //栅栏属于全局操作,执行栅栏操作可以影响到在线程中的其他原子操作
    //栅栏操作通常也被称为内存栅栏
    if (atomic_x.load(std::memory_order_relaxed)) {
        ++atomic_int;
    }
}
void func2() {
    atomic_x = false;
    atomic_y = false;
    atomic_int = 0;
    std::thread T1(fence_write_x_y);
    std::thread T2(fence_read_y_x);
    T1.join();
    T2.join();
    if (atomic_int.load() == 0) { std::cout << "atomic_int still equal 0\n"; }
    else { std::cout << "atomic_int now equal " << atomic_int.load() << "\n"; }
    //释放栅栏与获取栅栏同步,这是因为加载y的操作会读取存储的值
    //所以存储x先行于加载x,最后x读取出来必定为true,且断言不会被触发
    //原先不带栅栏的存储和加载x是无序的,并且断言是可能会触发
    //这两个栅栏都是必要的,需要在一个线程中进行释放,然后在另一个线程中进行获取,这样才能构建出同步关系
    //栅栏当获取操作能看到释放栅栏操作后的存储结果,那么这个栅栏就与获取操作同步
    //并且当加载操作在获取栅栏操作前
    //看到一个释放操作的结果,那么这个释放操作同步于获取栅栏
    //也可以使用双边栅栏操作,当一个加载操作在获取栅栏前,看到一个值有存储操作写入
    //且这个存储操作发生在释放栅栏后,那么释放栅栏与获取栅栏同步。
    //虽然栅栏同步依赖于读取/写入的操作发生于栅栏之前/后,但是这里有一点很重要,同步点(就是栅栏本身)
    //write_x_y如果改为
    /*  std::atomic_thread_fence(std::memory_order_release);
        x.store(true,std::memory_order_relaxed);
        y.store(true,std::memory_order_relaxed);
    */
    //这里的两个操作就不会被栅栏分开,并且也不再有序
    //只有当栅栏出现在存储x和存储y操作之间时,这个顺序才是硬性的

}

bool x2 = false;
std::atomic<bool> atomic_y2;
std::atomic<int> atomic_int2;
void write_x_y() {
    x2 = true;
    std::atomic_thread_fence(std::memory_order_release);
    atomic_y2.store(true, std::memory_order_relaxed);
}
void read_y_x() {
    while (!atomic_y2.load(std::memory_order_relaxed)) { ; }
    std::atomic_thread_fence(std::memory_order_acquire);
    if (x2) {
        ++atomic_int2;
    }
}
void func3() {
    x2 = false;
    atomic_y2 = false;
    atomic_int2 = 0;
    std::thread T1(write_x_y);
    std::thread T2(read_y_x);
    T1.join();
    T2.join();
    if (atomic_int2.load() == 0) { std::cout << "atomic_int still equal 0\n"; }
    else { std::cout << "atomic_int now equal " << atomic_int2.load() << "\n"; }
    //栅栏仍然为存储x和存储y,还有加载y和加载x提供一个执行序列
    //并且这里仍然有一个先行关系,在存储x和加载x之间,所以断言不会被触发
    //y中的存储和read_y_x对y的加载,都必须是原子操作
    //否则将会在y上产生条件竞争,不过一旦读取线程看到存储到y的操作,栅栏将会对x执行有序的操作
    //这个执行顺序意味着即使它被另外的线程修改或被其他线程读取,x上也不存在条件竞争
}
//C++内存模型的底层知识进行详尽的了解
//原子操作能在线程间提供同步,包含基本的原子类型,由std::atomic<>类模板和std::experimental::atomic_shared_ptr<>模板特化后提供的接口
//对于这些类型的操作,还有对内存序列选项的各种复杂细节都由std::atomic<>类模板提供。
//栅栏让执行序列中,对原子类型的操作成对同步
//原子操作也可以在不同线程上的非原子操作间使用,并进行有序执行


int main() {
    func3();
}