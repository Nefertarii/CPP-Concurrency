函数指针  
int plus(int a, int b) { return a + b; }
using Plus = int (*)(int, int);
//C所保留的用法

仿函数
class Plus {
public:
    int operator()(int a, int b) { return a + b; }
};
//让类实现 operator()

lambda
int Plus = [](int a, int b){ return a + b; }

c++ lambda
[capture-list] (params) mutable(optional) exception(optional) attribute(optional) -> ret(optional) { body }
[capture list]          //以[]开始，用于捕获变量
(params list)           //参数
mutable(optional)       //
exception //异常标识
attribute(optional)     //属性标识
ret(optional)           //返回类型
{ function body }       //{}其中的内容作为lambda的主体
()                      //可作为lambda的结束，通过该括号直接调用



