#ifndef SINGLETON_H
#define SINGLETON_H

// 单例实现
#include <global.h>

template <typename T>
class Singleton{
protected:
    Singleton() = default; // protected Singleton()：使得只能被继承（不能直接实例化 Singleton<T>），子类 T 可以访问构造函数。
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator = (const Singleton<T>& st) = delete;
    static std::shared_ptr<T> _instance; // 类模板的静态成员，每个不同的 T 类型都有独立的 _instance 实例。
public:
    static std::shared_ptr<T> GetInstance(){
        // 使用 std::once_flag 和 std::call_once 确保无论多少线程同时访问，构造函数只被调用一次。
        static std::once_flag s_flag;
        std::call_once(s_flag, [&](){
            _instance = std::shared_ptr<T>(new T); // 为什么不用make_shared
        });

        return _instance;
    }

    void PrintAddress(){
        std::cout << _instance.get() << std::endl;
    }

    ~Singleton(){
        std::cout << "This is singleton destruct" << std::endl;
    }
};

template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr; // 类模板的静态成员必须在类外初始化。

#endif // SINGLETON_H
