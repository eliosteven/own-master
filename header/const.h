#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include "Singleton.h"
#include <functional>
#include <map>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <atomic>
#include <queue>
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>
#include <mutex>
#include <condition_variable>
#include <hiredis/hiredis.h>
#include <cassert>
#include <memory>


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;

enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,      // JSON parse error
	RPCFailed = 1002,       // RPC request error
    VarifyExpired = 1003,   // Varified code expired
    VarifyCodeErr = 1004,   // Varified code error
    UserExist = 1005,       // User has existed
    PasswdErr = 1006,       // Password error
    EmailNotMatch = 1007,   // Email not match
    PasswdUpFailed = 1008,  // Update password failed
    PasswdInvalid = 1009,   // Password invalid
};

// Defer类
class Defer {
public:
	// 接受一个lambda表达式或者函数指针
	Defer(std::function<void()> func) : func_(func) {}

	// 析构函数中执行传入的函数
	~Defer() {
		func_();
	}

private:
	std::function<void()> func_;
};

#define CODEPREFIX "code_"