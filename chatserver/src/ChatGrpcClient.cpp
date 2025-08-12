#include "ChatGrpcClient.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "UserMgr.h"

#include "CSession.h"
#include "MysqlMgr.h"

ChatGrpcClient::ChatGrpcClient()
{
	auto& cfg = ConfigMgr::Inst();
	auto server_list = cfg["PeerServer"]["Servers"];

	std::vector<std::string> words;

	std::stringstream ss(server_list);
	std::string word;

	while (std::getline(ss, word, ',')) {
		words.push_back(word);
	}

	for (auto& word : words) {
		if (cfg[word]["Name"].empty()) {
			continue;
		}
		_pools[cfg[word]["Name"]] = std::make_unique<ChatConPool>(5, cfg[word]["Host"], cfg[word]["Port"]);
	}

}

AddFriendRsp ChatGrpcClient::NotifyAddFriend(std::string server_ip, const AddFriendReq& req)
{
	AddFriendRsp rsp;
	Defer defer([&rsp, &req]() {
		rsp.set_error(ErrorCodes::Success);
		rsp.set_applyuid(req.applyuid());
		rsp.set_touid(req.touid());
		});

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) {
		return rsp;
	}
	
	auto &pool = find_iter->second;
	ClientContext context;
	auto stub = pool->getConnection();
	Status status = stub->NotifyAddFriend(&context, req, &rsp);
	Defer defercon([&stub, this, &pool]() {
		pool->returnConnection(std::move(stub));
		});

	if (!status.ok()) {
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}

	return rsp;
}


bool ChatGrpcClient::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
{
    std::string info_str;
    bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);

    if (b_base) {
        json root = json::parse(info_str, /*cb=*/nullptr, /*allow_exceptions=*/false);
        if (!root.is_discarded() && root.is_object()) {
            if (!userinfo) userinfo = std::make_shared<UserInfo>();

            userinfo->uid   = root.value("uid",   0);
            userinfo->name  = root.value("name",  std::string{});
            userinfo->pwd   = root.value("pwd",   std::string{});
            userinfo->email = root.value("email", std::string{});
            userinfo->nick  = root.value("nick",  std::string{});
            userinfo->desc  = root.value("desc",  std::string{});
            userinfo->sex   = root.value("sex",   0);
            userinfo->icon  = root.value("icon",  std::string{});

            std::cout << "user login uid is " << userinfo->uid
                      << " name is " << userinfo->name
                      << " pwd is " << userinfo->pwd
                      << " email is " << userinfo->email << endl;

            return true; // 命中 Redis 且解析成功，直接返回
        }
        // 若 Redis 命中但 JSON 异常，继续走 DB 回源
    }

    // DB 回源
    std::shared_ptr<UserInfo> user_info = MysqlMgr::GetInstance()->GetUser(uid);
    if (!user_info) {
        return false;
    }
    userinfo = user_info;

    // 写回 Redis（替换 Json::Value 为 nlohmann::json）
    json redis_root = {
        {"uid",   uid},
        {"pwd",   userinfo->pwd},
        {"name",  userinfo->name},
        {"email", userinfo->email},
        {"nick",  userinfo->nick},
        {"desc",  userinfo->desc},
        {"sex",   userinfo->sex},
        {"icon",  userinfo->icon}
    };
    RedisMgr::GetInstance()->Set(base_key, redis_root.dump()); // dump(2) 也可用于美化

    return true; // 原代码这里缺少返回值，会产生未定义行为；已修复
}

AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req)
{
    AuthFriendRsp rsp;
    rsp.set_error(ErrorCodes::Success);

    Defer defer([&rsp, &req]() {
        rsp.set_fromuid(req.fromuid());
        rsp.set_touid(req.touid());
    });

    auto find_iter = _pools.find(server_ip);
    if (find_iter == _pools.end()) {
        return rsp;
    }

    auto& pool = find_iter->second;
    ClientContext context;
    auto stub = pool->getConnection();
    Status status = stub->NotifyAuthFriend(&context, req, &rsp);
    Defer defercon([&stub, this, &pool]() {
        pool->returnConnection(std::move(stub));
    });

    if (!status.ok()) {
        rsp.set_error(ErrorCodes::RPCFailed);
        return rsp;
    }

    return rsp;
}

TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(std::string server_ip,
                                                 const TextChatMsgReq& req,
                                                 const json& rtvalue)
{
    TextChatMsgRsp rsp;
    rsp.set_error(ErrorCodes::Success);

    Defer defer([&rsp, &req]() {
        rsp.set_fromuid(req.fromuid());
        rsp.set_touid(req.touid());
        for (const auto& text_data : req.textmsgs()) {
            TextChatData* new_msg = rsp.add_textmsgs();
            new_msg->set_msgid(text_data.msgid());
            new_msg->set_msgcontent(text_data.msgcontent());
        }
    });

    // 如果需要从 rtvalue 读取可选控制字段，这里示例几种安全访问用法：
    // auto scene = rtvalue.value("scene", std::string{});
    // int64_t msg_id = rtvalue.value("msg_id", 0);

    auto find_iter = _pools.find(server_ip);
    if (find_iter == _pools.end()) {
        return rsp;
    }

    auto& pool = find_iter->second;
    ClientContext context;
    auto stub = pool->getConnection();
    Status status = stub->NotifyTextChatMsg(&context, req, &rsp);
    Defer defercon([&stub, this, &pool]() {
        pool->returnConnection(std::move(stub));
    });

    if (!status.ok()) {
        rsp.set_error(ErrorCodes::RPCFailed);
        return rsp;
    }

    return rsp;
}

KickUserRsp ChatGrpcClient::NotifyKickUser(std::string server_ip, const KickUserReq& req)
{
	KickUserRsp rsp;
	Defer defer([&rsp, &req]() {
		rsp.set_error(ErrorCodes::Success);
		rsp.set_uid(req.uid());
		});

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) {
		return rsp;
	}

	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->getConnection();
	Defer defercon([&stub, this, &pool]() {
		pool->returnConnection(std::move(stub));
		});
	Status status = stub->NotifyKickUser(&context, req, &rsp);

	if (!status.ok()) {
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}

	return rsp;
}
