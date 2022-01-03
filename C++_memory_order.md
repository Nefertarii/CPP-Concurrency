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

#### 多线程下的内存序
在编译和CPU执行的时候,代码的执行可能是会被乱序  
内存序是希望以什么样的顺序把代码执行的结果在任意时刻呈现给别的线程看    
1. 无承诺的保证 std::memory_order_relax   
这种模式与没有使用这种模式达到的效果是一样的  

2. 写顺序的保证 std::memory_order_release  
在本行代码之前,如果有任何写内存的操作,都是不能放到本行语句之后的  

3. 读顺序的保证 std::memory_order_acquire  
后续的读操作都不能放到这条指令之前  

4. 读顺序的消弱 std::memory_order_consume
所有后续对本原子类型的操作，必须在本操作完成之后才可以执行
consume只是要求依赖于consume这条语句的读写不得乱序

5. 读写的加强 std::memory_order_acq_rel
对本条语句的读写进行约束,如果有任何写内存的操作,都不能放到本行语句之后,
后续的读操作都不能放到这条指令之前

6. 最强的约束 std::memory_order_seq_cst
在执行本行代码时,所有这条指令前面的语句不能放到后面,所有这条语句后面的语句不能放到前面来执行

#### 整理来源   
https://preshing.com/20140709/the-purpose-of-memory_order_consume-in-cpp11/
https://www.zhihu.com/question/24301047/answer/83422523  
https://en.cppreference.com/w/cpp/atomic/memory_order  
https://zhuanlan.zhihu.com/p/55901945