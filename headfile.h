#include <thread>
#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <numeric>      //accumulate
#include <functional>   //mem_fn
#include <utility>      //forward
#include <stdarg.h>     //... var

#include <list>
#include <stack>
#include <queue>
#include <map>
#include <mutex>        //mutex,lock_guard,lock,scoped_lock
#include <shared_mutex> //only in c++14,c++17 
#include <exception>    
#include <memory>
#include <algorithm>


using Ulong = unsigned long;

int plus(int a, int b) { return a + b; }

void do_something() { std::cout << "print something.\n"; }
void do_something_else() { std::cout << "print another.\n"; }
void print_something(int i) { std::cout << "print" << i << "\n"; }
void thread_do_something(int& i, std::string& str) { std::cout << "num:" << i << " string:" << str << "\n"; }
void do_something_in_current_thread() { std::cout << "thread current do someting...\n"; }