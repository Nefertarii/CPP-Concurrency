#include "headfile.h"

#include <experimental/future>

//C++实验性内容 std::experiment
//std::experimental::future 持续性并发
void func1() {
    std::experimental::future<int> find_the_answer;
}




int main() {
    func1();
}