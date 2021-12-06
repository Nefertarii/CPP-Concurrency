#include <chrono>
#include <unistd.h>
#include <iostream>

using Us = std::chrono::microseconds;
using Ms = std::chrono::milliseconds;
using Sec = std::chrono::seconds;
using SysClock = std::chrono::system_clock;
using HighClock = std::chrono::high_resolution_clock;
template<typename Rep, typename Period>
using Duration = std::chrono::duration<Rep, Period>;
template<typename Clock, typename Duration>
using TimePoint = std::chrono::time_point<Clock, Duration>;

//时钟为一个类
//提供当前时间,时间类型,时钟节拍,通过节拍判断时钟稳定
void func1() {
    std::chrono::system_clock::now();
    //当前系统时钟的时间为一个静态成员函数 返回为time_point类型

    //时钟节拍为 1/x秒 如
    std::ratio<1, 25> rat1;
    //表示一个时钟一秒有25个节拍
    std::ratio<5, 2> rat2;
    //表示一个时钟每2.5秒一个节拍

    //如果当前钟节拍均匀分布(无论是否与周期匹配),并且不可调整,这种时钟就称为稳定时钟
    //当is_steady静态数据成员为true时,表明这个时钟就是稳定的 否则,就是不稳定的
    //系统时钟通常是不稳定的,C++提供std::chrono::steady_clock作为一个稳定时钟
}

//std::chrono::duration<rep, period> 模板用于延时,表示一段时间
//其第一个参数为一个类型,第二个参数为每一个单元所用的秒数
void func2() {
    //同时有一些预定义类型用于表示时间
    //nanoseconds,microseconds,milliseconds,seconds,minutes,hours
    //如需要将几分钟的时间储存在int类型中
    std::chrono::duration<int, std::ratio<60, 1>> time1;
    //需要将毫秒级计数存在double中时
    std::chrono::duration<double, std::ratio<1, 1000>> time2;
}


//time_point<Clock,Duration>
//时间点time_point模板 第一个参数指定所需的时钟,第二个参数用于表示时间的单位
void func3() {
    //时间点可用于加减
    std::chrono::time_point point1 = std::chrono::high_resolution_clock::now();
    sleep(2);
    std::chrono::time_point point2 = std::chrono::high_resolution_clock::now();
    std::cout << "Thread sleep "
        << Duration<double, std::ratio<1, 1000>>(point2 - point1).count()
        << " Ms.\n";
    
}

void func4() {
    
}

int main() {
    func3();
}