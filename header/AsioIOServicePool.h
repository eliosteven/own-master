#pragma once
#include <vector>
#include <boost/asio.hpp>
#include "Singleton.h"
class AsioIOServicePool:public Singleton<AsioIOServicePool>
{
    friend Singleton<AsioIOServicePool>;
public:
    using IOService = boost::asio::io_context;
    //using Work = boost::asio::io_context::work;
    using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;
    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool&) = delete;
    AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;
    // 使用 round-robin 的方式返回一个 io_service
    boost::asio::io_context& GetIOService();    //采用轮询的方式选出一个 io_context 供用户注册异步任务
    void Stop();    //用于释放所有线程和异步服务资源
private:
    AsioIOServicePool(std::size_t size = 2/*std::thread::hardware_concurrency()*/);
    std::vector<IOService> _ioServices; //每个负责一个线程内的任务调度
    std::vector<WorkPtr> _works;
    std::vector<std::thread> _threads;
    std::size_t                        _nextIOService;  //轮询选择哪个 io_context 给用户使用
};