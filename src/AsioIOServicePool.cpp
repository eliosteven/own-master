#include "AsioIOServicePool.h"
#include <iostream>
using namespace std;
AsioIOServicePool::AsioIOServicePool(std::size_t size):_ioServices(size),
_works(size), _nextIOService(0){    //不能支持拷贝的，都要放在初始化列表中
    for (std::size_t i = 0; i < size; ++i) {
        _works[i] = std::make_unique<Work>(_ioServices[i].get_executor());
    }

    //遍历多个ioservice，创建多个线程，每个线程内部启动ioservice
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
            });
    }
}

AsioIOServicePool::~AsioIOServicePool() {
    Stop();
    std::cout << "AsioIOServicePool destruct" << endl;
}

boost::asio::io_context& AsioIOServicePool::GetIOService() {
    auto& service = _ioServices[_nextIOService++];
    if (_nextIOService == _ioServices.size()) {
        _nextIOService = 0;
    }
    return service;
}

void AsioIOServicePool::Stop(){
    // 先释放 work_guard，让 run() 有机会返回
    for (auto& work : _works) {
        if (work) {
            work->reset();  // 释放 keep-alive
        }
    }

    // 主动停止 io_context，以确保即使还有未完成任务也能中断退出
    for (auto& ctx : _ioServices) {
        ctx.stop();
    }

    // 回收所有线程
    for (auto& t : _threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}