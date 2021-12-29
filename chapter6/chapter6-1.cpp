#include "../headfile.h"

//使用互斥量,条件变量以及期望值可以用来同步阻塞算法和数据结构
//调用库函数将会挂起执行线程,直到其他线程完成某个特定的动作
//库函数将调用阻塞操作来对线程进行阻塞,在阻塞移除前线程无法继续自己的任务
//通常操作系统会完全挂起一个阻塞线程(并将其时间片交给其他线程),直到其被其他线程解阻塞
//解阻塞”的方式很多,比如解锁一个互斥锁,通知条件变量达成,或让 期望 就绪。
//不使用阻塞库的数据结构和算法被称为无阻塞结构


//无阻碍——如果所有其他线程都暂停了，任何给定的线程都将在一定时间内完成其操作。
//无锁——如果多个线程对一个数据结构进行操作，经过一定时间后，其中一个线程将完成其操作。
//无等待——即使有其他线程也在对该数据结构进行操作，每个线程都将在一定的时间内完成其操作
//自旋锁没有调用任何阻塞函数,但自旋锁并不是无锁结构
//自旋锁的代码自旋于循环当中
//所以没有阻塞调用,任意代码使用互斥量来保护共享数据都是非阻塞的

//使用无锁结构的主要原因是能将并发最大化
//使用基于锁的容器，会让线程阻塞或等待 互斥锁削弱了结构的并发性
//而无锁数据结构中,某些线程可以逐步执行
//无锁数据结构的鲁棒性,当线程在无锁数据结构上执行操作,在执行到一半终止时,数据结构上的数据没有丢失(除了线程本身的数据),其他线程依旧可以正常执行
//无锁-无等待代码的缺点:虽然提高了并发访问的能力,减少了单个线程的等待时间,但是其可能会将整体性能拉低
//首先原子操作的无锁代码要慢于无原子操作的代码,原子操作就相当于无锁数据结构中的锁
//不仅如此,硬件必须通过同一个原子变量对线程间的数据进行同步

//无锁的线程安全栈
int const max_hazard_point = 100;
struct hazard_point {
    std::atomic<std::thread::id> id;
    std::atomic<void*> point;
};
hazard_point hazard_pointer[max_hazard_point];
class hazard_owner {
private:
    hazard_point* hp;
public:
    hazard_owner(hazard_owner  const&) = delete;
    hazard_owner operator=(hazard_owner const&) = delete;
    hazard_owner() :hp(nullptr) {
        for (int i = 0;i < max_hazard_point;i++) {
            std::thread::id old_id;
            if (hazard_pointer[i].id.compare_exchange_strong(
                old_id, std::this_thread::get_id())) {
                hp = &hazard_pointer[i];
                break;
            }//尝试获取风险指针的所有权
        }
        if (!hp) { throw std::runtime_error("No hazard point available."); }
    }
    std::atomic<void*>& get_point() { hp->point; }
    ~hazard_owner() {
        hp->point.store(nullptr);
        hp->id.store(std::thread::id());
    }
};
std::atomic<void*>& get_hazard_point() {
    thread_local static hazard_owner hazard;
    return hazard.get_point();
}//创建一个缓存表方便查询
bool outstanding_hazard_point(void* p) {
    for (int i = 0;i < max_hazard_point;i++) {
        if (hazard_pointer[i].point.load() == p) { return true; }
    }
    return false;
}//对风险指针进行搜索,空指针即是没有所有者的指针
template<typename T>
void do_delete(void* p) { delete static_cast<T*>(p); }
struct data_to_reclaim {
    void* data;
    std::function<void(void*)> deleter;
    data_to_reclaim* next;
    template<typename T>
    data_to_reclaim(T* p) :data(p), deleter(&do_delete<T>), next(0) {}
    ~data_to_reclaim() { deleter(data); }
};
std::atomic<data_to_reclaim*> node_to_reclaim;
void add_to_reclaim_list(data_to_reclaim* node) {
    node->next = node_to_reclaim.load();
    while (!node_to_reclaim.compare_exchange_weak(node->next, node));
}
void delete_no_hazard_node() {
    data_to_reclaim* current = node_to_reclaim.exchange(nullptr);
    while (current) {
        data_to_reclaim* const next = current->next;
        if (!outstanding_hazard_point(current->data)) { delete current; }
        else { add_to_reclaim_list(current); }
        current = next;
    }
}//删除无记录的指针



template <typename T>
class lock_free_stack {
private:
    struct node {
        T data;
        node* next;
        node(T const& data_) :data(data_) {}
    };
    std::atomic<node*> head;
    std::atomic<int> thread_pop;
    std::atomic<node*> to_deleted;
    static void delete_nodes(node* nodes) {
        while (nodes) {
            node* next = nodes->next;
            delete nodes;
            nodes = next;
        }
    }
    void chain_pending_nodes(node* nodes) {
        node* last = nodes;
        while (node* const next = last->next) { last = next; }
        chain_pending_nodes(nodes, last);
    }
    void chain_pending_nodes(node* first, node* last) {
        last->next = to_deleted;
        while (!to_deleted.compare_exchange_weak(last->next, first));
    }
    void chain_pending_node(node* n) {
        chain_pending_nodes(n, n);
    }
    void try_reclaim(node* old_head) {
        if (thread_pop == 1) {
            node* node_t = to_deleted.exchange(nullptr);//通过原子操作删除列表
            if (!(--thread_pop)) { delete_nodes(node_t); }//计数器为0即可删除 确保没有其他线程正在调用pop
            else if (to_deleted) { chain_pending_nodes(old_head); }//计数器不为0 则需要添加到删除链表之后等待
            delete old_head; //此时引用计数为1即可删除 计数器为1表示只有当前线程在使用
        }
        else {
            chain_pending_node(old_head);//引用计数不为1 向等待列表中添加
            --thread_pop;
        }
    }
public:
    void push(T const& data) {
        //添加节点 创建新节点,新节点next指向当前head节点,head节点指向新节点
        node* const new_node = new node(data);
        //
        new_node->next = head.load();
        while (!head.compare_exchange_weak(new_node->next, new_node));
        //使用比较/交换操作在返回false时,因为比较失败(例如，head被其他线程锁修改)
        //会使用head中的内容更新new_node->next(第一个参数)的内容
    }
    //为了保证多线程调用栈时能正确使用,线程调用pop时先放入可删除列表中,直到在没有线程对pop调用时再进行删除
    //删除节点 读取当前head指针的值,读取head->next
    //设置head到head->next,通过索引node返回data数据,删除索引节点
    std::shared_ptr<T> count_pop() {
        ++thread_pop;//计数器记录调用的线程数量
        node* old_head = head.load();
        while (old_head && !head.compare_exchange_weak(old_head, old_head->next));
        std::shared_ptr<T> res;
        if (old_head) { res.swap(old_head->data); }//回收删除的节点
        try_reclaim(old_head);//直接使用数据,而不拷贝指针
        return res;
    }
    std::shared_ptr<T> hazard_pop() {
        std::atomic<void*>& hazard_point = get_hazard_point();
        node* old_head = head.load();
        do {
            node* tmp;
            do {
                tmp = old_head;
                hazard_point.store(old_head);
                old_head = head.load();
            } while (old_head != tmp);
            //循环内部会对风险指针进行设置,compare_exchange操作失败时会重载old_head,并再次尝试设置风险指针
            //因为需要在循环内部做一些实际的工作,所以要使用compare_exchange_strong()
            //当compare_exchange_weak()伪失败后, 风险指针将被重置
        } while (old_head && !head.compare_exchange_strong(old_head, old_head->next));
        //在比较交换失败后再次设置head指针 直到成功将head设置为风险指针
        hazard_point.store(nullptr);//声明完成后即可清除
        std::shared_ptr<T> res;
        if (old_head) {
            res.swap(old_head->data);
            if (outstanding_hazard_point(old_head)) {
                //检查是否有对风险指针的引用
                reclaim_later(old_head);
                //没有,回收
            }
            else {
                delete old_head;
            }
            delete_no_hazard_node();
            //链表上没有风险指针引用节点即可删除
        }
        return res;
    }
};
//风险指针(书上说用处不大)
//当有线程去访问要被(其他线程)删除的对象时,会先设置对这个对象设置风险指针,而后通知其他线程使用这个指针是危险的行为
//当这个对象不再被需要,那么就可以清除风险指针了
//当线程想要删除一个对象,就必须检查系统中其他线程是否持有风险指针
//当没有风险指针时,就可以安全删除对象,否则就必须等待风险指针消失


int main() {
    lock_free_stack<int> stk;
}