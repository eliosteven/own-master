#include "LogicSystem.h"
#include "HttpConnection.h"
#include "VarifyGrpcClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"

void LogicSystem::RegGet(std::string url, HttpHandler handler) {
	_get_handlers.insert(make_pair(url, handler));
}

void LogicSystem::RegPost(std::string url, HttpHandler handler) {
	_post_handlers.insert(make_pair(url, handler));
}

LogicSystem::LogicSystem() {
	RegGet("/get_test", [](std::shared_ptr<HttpConnection> connection) {
		beast::ostream(connection->_response.body()) << "receive get_test req" << std::endl;
		int i = 0;
		for (auto& elem : connection->_get_params) {
			i++;
			beast::ostream(connection->_response.body()) << "param " << i << " key is " << elem.first;
			beast::ostream(connection->_response.body()) << ", " <<  "value is " << elem.second << std::endl;
		}
	});

	RegPost("/get_varifiedcode", [](std::shared_ptr<HttpConnection> connection) {
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "received body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type, "application/json");

        // 替换 JsonCpp 解析逻辑
        json src_root;
        try {
            src_root = json::parse(body_str); // 解析 JSON
        }
        catch (const json::parse_error& e) {
            std::cout << "Failed to parse JSON data: " << e.what() << std::endl;
            json root;
            root["error"] = ErrorCodes::Error_Json;
            beast::ostream(connection->_response.body()) << root.dump();
            return true;
        }

        // 替换 isMember 检查
        if (!src_root.contains("email")) { // 等效于 isMember
            std::cout << "Missing 'email' field in JSON!" << std::endl;
            json root;
            root["error"] = ErrorCodes::Error_Json;
            beast::ostream(connection->_response.body()) << root.dump();
            return true;
        }

        // 获取字段值（等效于 asString）
        auto email = src_root["email"].get<std::string>();
        GetVarifyRsp rsp = VerifyGrpcClient::GetInstance()->GetVarifyCode(email);
        std::cout << "email is " << email << std::endl;

        // 构建响应 JSON
        json root;
        root["error"] = rsp.error();
        root["email"] = email; // 直接赋值，无需 asString
        beast::ostream(connection->_response.body()) << root.dump();
        return true;

	});

    RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type, "text/json");
        json src_root;
        try{
            src_root = json::parse(body_str);
        } catch (const json::parse_error& e) {
            std::cout << "Failed to parse JSON data!" << std::endl;
            json root;
            root["error"] = ErrorCodes::Error_Json;
            beast::ostream(connection->_response.body()) << root.dump();
            return true;
        }

        std::string email = src_root["email"].get<std::string>();
        std::string user = src_root["user"].get<std::string>();
        std::string passwd = src_root["passwd"].get<std::string>();
        std::string confirm = src_root["confirm"].get<std::string>();
        std::string varifycode = src_root["varifycode"].get<std::string>();
        
        json root;

        if (passwd != confirm) {
            std::cout << "password err " << std::endl;
            root["error"] = ErrorCodes::PasswdErr;
            beast::ostream(connection->_response.body()) << root.dump();
            return true;
        }

        //先查找redis中email对应的验证码是否合理
        std::string varify_code;
        bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + email, varify_code);
        if (!b_get_varify) {
            std::cout << " get varify code expired" << std::endl;
            root["error"] = ErrorCodes::VarifyExpired;
            beast::ostream(connection->_response.body()) << root.dump();
            return true;
        }

        if (varify_code != varifycode) {
            std::cout << " varify code error" << std::endl;
            root["error"] = ErrorCodes::VarifyCodeErr;
            beast::ostream(connection->_response.body()) << root.dump();
            return true;
        }

        // TODO: 数据库mysql中检查用户是否存在（如果你实现了DB查询）
        //查找数据库判断用户是否存在
        int uid = MysqlMgr::GetInstance()->RegUser(user, email, passwd);
        if (uid == 0 || uid == -1) {
            std::cout << " user or email exist" << std::endl;
            root["error"] = ErrorCodes::UserExist;
            beast::ostream(connection->_response.body()) << root.dump();
            return true;
        }
        root["error"] = 0;
        root["email"] = email;
        root["uid"] = uid;
        root["user"] = user;
        root["passwd"] = passwd;
        root["confirm"] = confirm;
        root["varifycode"] = varifycode;
        beast::ostream(connection->_response.body()) << root.dump();
        return true;
    });
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con) {
	if (_get_handlers.find(path) == _get_handlers.end()) {
		return false;
	}

	_get_handlers[path](con);
	return true;
}


bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> con) {
	if (_post_handlers.find(path) == _post_handlers.end()) {
		return false;
	}

	_post_handlers[path](con);
	return true;
}