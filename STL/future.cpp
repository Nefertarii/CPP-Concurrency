#include <iostream>
#include <thread>
#include <exception>
#include <ref>
#include <future>

/* future类
 * 能提供访问异步操作的结果
 * 该类禁止了拷贝构造函数,只能使用默认构造函数和move构造函数,同时不能普通赋值,只能通过move赋值
 * 通过 get() 等待异步操作结束并返回结果
 *      wait() 只等待异步操作结束,不返回结果
 *      wait_for() 设置一个时间,在等待了该时间后,返回结果
 * 获取future的状态以获得结果 共有三种状态
 * defereed:异步操作还未开始
 * ready:异步操作已经完成
 * timeout:异步操作超时
 */



/* promise类
 * 保存了一个类型T的值,同时可供不同线程的future对象读取
 * 该类禁止拷贝
 * 通过 get_future() 获取与该promise对象相关联的future的
 * 返回的future可以访问promise设置在共享状态上的值/异常
 *      set_value() 设置共享状态的值,状态标志置为ready
 *      set_excepiton() 为该promise设置异常,状态标志置为ready
 *      swap() 交换两个promise的共享状态
 */