#### 理解原子锁需要足够了解C++的内存相关

#### C++11中有3种不同类型的同步语义和执行序列约束
因为cache的存在,虽然前面更新了变量的值,但是可能存在某个CPU的缓存中,而其他CPU的缓存还是原来的值,这样就可能产生错误,定义内存顺序就可以强制性的约束一些值的更新 

1. 顺序一致性(Sequential consistency)    
对应的内存模型是 memory_order_seq_cst    
指明的是在线程间,建立一个全局的执行序列    
在此情况下,所有线程执行指令的顺序都是按照源代码顺序的,且每个线程所能看到的其他线程的操作的执行顺序都是一样的

2. 请求-释放(Acquire-release)   
对应的内存模型是 memory_order_consume, memory_order_acquire, memory_order_release, memory_order_acq_rel   
在线程间的同一个原子变量的读和写操作上建立一个执行序列   
此情况下,所有读/写操作不能移至acquire(获取)之前,release(释放)之后   

3. 松散型(Relaxed) 非严格约束   
对应的内存模型是 memory_order_relaxed   
只保证在同一个线程内，同一个原子变量的操作的执行序列不会被重排序，但是其他线程看到的这些操作的执行序列是不同的  
此情况下,对操作没有约束   

#### Memory coherence and memory consistency
consistency 和 coherence 的中文翻译近似，但这两个内存储存模型实际差别较大

coherence 保证了大家看到的内存是一致的  
关注在发生了什么事件后,写的数据对他人可见  
定义了一个读操作能获得什么样的值  
consistent 说一个处理器对不同数据的读写,在其他不同的处理器看来是否满足某些性质  
关注的是所有人对某个人对同一个变量的读写是否满足某种性质  
定义了何时一个写操作的值会被读操作获得   

满足如下三个条件的内存系统是为coherent
1. 对于同一地址 X, 若处理器 P 在写后读，并且在 P 的写后没有别的处理器写 X，那么 P 的读总是能返回 P 写入的值。条件1要求系统提供我们在单处理器中已熟知的程序次序(program order)  
2. 处理器 P1 完成对 X 的写后，P2 读 X. 如果两者之间相隔时间足够长，并且没有其他处理器写 X, 那么 P2 可以获得 P1 写入的值。条件2要求写结果最终对别的处理器可见。
3. 对同一地址的写操作是串行的(serialized), 在所有处理器看来，通过不同处理器对X的写操作都是以相同次序进行的。举个例子，如果值 1 先被写入，值 2 后被写入，那么处理器不能先读到值 2, 然后再读到值 1.性质3要求系统提供写串行化(write serialization). 若这一性质不被满足，程序错误可能会被引发：考虑 P1 先写 P2 后写的情况，如果存在一些处理器先看到P2的写结果，后看到 P1 的值，那么这个处理器将无限期地保持P1写入的值

#### C++提供同步的一些工具
这些机制都会为同步关系之间顺序进行保证。这样就可以使用它们进行数据同步，并保证同步关系间的顺序  
1. std::thread  
std::thread构造新线程时，构造函数与调用函数或新线程的可调用对象间的同步。  
对std::thread对象调用join，可以和对应的线程进行同步。  
2. std::mutex, std::timed_mutex, std::recursive_mutex, std::recursibe_timed_mutex    
对给定互斥量对象的lock和unlock的调用，以及对try_lock，try_lock_for或try_lock_until的成功调用，会形成该互斥量的锁序。  
对给定的互斥量调用unlock，需要在调用lock或成功调用try_lock，  try_lock_for或try_lock_until之后，这样才符合互斥量的锁序。  
对try_lock，try_lock_for或try_lock_until失败的调用，不具有任何同步关系。  
3. std::shared_mutex , std::shared_timed_mutex   
对给定互斥量对象的lock、unlock、lock_shared和unlock_shared的调用，以及对 try_lock , try_lock_for , try_lock_until , try_lock_shared , try_lock_shared_for或 try_lock_shared_until的成功调用，会形成该互斥量的锁序。  
对给定的互斥量调用unlock，需要在调用lock或shared_lock，亦或是成功调用try_lock , try_lock_for, try_lock_until, try_lock_shared, try_lock_shared_for或try_lock_shared_until之后，这样才符合互斥量的锁序。  
对try_lock，try_lock_for，try_lock_until，try_lock_shared，try_lock_shared_for或try_lock_shared_until l失败的调用，不具有任何同步关系。 
4. std::shared_mutex和std::shared_timed_mutex      
给定std::promise对象成功的调用set_value或set_exception与成功的调用wait或get之间同步，或是调用wait_for或wait_until的返回期望值状态std::future_status::ready与承诺值共享同步状态。  
给定std::promise对象的析构函数，该对象存储了一个std::future_error异常，其共享同步状态与承诺值之间的同步在于成功的调用wait或get，或是调用wait_for或wait_until返回的期望值状态std::future_status::ready与承诺值共享同步状态。  
5. std::packaged_task , std::future和std::shared_future  
给定std::packaged_task对象成功的调用函数操作符与成功的调用wait或get之间同步，或是调用wait_for或wait_until的返回期望值状态   std::future_status::ready与打包任务共享同步状态。    
给定std::packaged_task对象的析构函数，该对象存储了一个std::future_error异常，其共享同步状态与打包任务之间的同步在于成功的调用wait或get，或是调用wait_for或wait_until返回的期望值状态  std::future_status::ready与打包任务共享同步状态。  
6. std::async , std::future和std::shared_future   
使用std::launch::async策略性的通过std::async启动线程执行任务与成功的调用wait和get之间是同步的，或调用wait_for或wait_until返回的期望值状态std::future_status::ready与产生的任务共享同步状态。  
使用std::launch::deferred策略性的通过std::async启动任务与成功的调用wait和get之间是同步的，或调用wait_for或wait_until返回的期望值状态std::future_status::ready与承诺值共享同步状态。  
std::experimental::future , std::experimental::shared_future和持续性  
异步共享状态变为就绪的事件与该共享状态上调度延续函数的调用同步。  
持续性函数的完成与成功调用wait或get的返回同步，或调用wait_for或wait_until返回的期望值状态std::future_status::ready与调用then构建的持续性返回的期望值同步，或是与在调度用使用这个期望值的操作同步。  
7. std::experimental::latch   
对给定std::experimental::latch实例调用count_down或count_down_and_wait与在该对象上成功的调用wait或count_down_and_wait之间是同步的。  
8. std::experimental::barrier   
对给定std::experimental::barrier实例调用arrive_and_wait或arrive_and_drop与在该对象上随后成功完成的arrive_and_wait之间是同步的。  
9. std::experimental::flex_barrier  
对给定std::experimental::flex_barrier实例调用arrive_and_wait或arrive_and_drop与在该对象上随后成功完成的arrive_and_wait之间是同步的。  
对给定std::experimental::flex_barrier实例调用arrive_and_wait或arrive_and_drop与在该对象上随后完成的给定函数之间是同步的。  
对给定std::experimental::flex_barrier实例的给定函数的返回与每次对arrive_and_wait的调用同步，当调用给定函数线程会在栅栏处阻塞等待。  
10. std::condition_variable和std::condition_variable_any  
条件变量不提供任何同步关系。它们是对忙等待循环的优化，所有同步都是相关互斥量提供的操作。  

#### 整理来源
https://zhuanlan.zhihu.com/p/58589781   
https://www.zhihu.com/question/24301047/answer/83422523  
https://backwit.github.io/2018/12/11/C++%20%E5%B9%B6%E8%A1%8C%E7%BC%96%E7%A8%8B%E4%B9%8B%E5%8E%9F%E5%AD%90%E6%93%8D%E4%BD%9C%E7%9A%84%E5%86%85%E5%AD%98%E9%A1%BA%E5%BA%8F/ 
https://en.cppreference.com/w/cpp/atomic/memory_order  