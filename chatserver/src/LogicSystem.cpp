#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"
#include "DistLock.h"
#include <string>
#include "CServer.h"
using namespace std;

LogicSystem::LogicSystem():_b_stop(false), _p_server(nullptr){
	RegisterCallBacks();
	_worker_thread = std::thread (&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem(){
	_b_stop = true;
	_consume.notify_one();
	_worker_thread.join();
}

void LogicSystem::PostMsgToQue(shared_ptr < LogicNode> msg) {
	std::unique_lock<std::mutex> unique_lk(_mutex);
	_msg_que.push(msg);
	if (_msg_que.size() == 1) {
		unique_lk.unlock();
		_consume.notify_one();
	}
}


void LogicSystem::SetServer(std::shared_ptr<CServer> pserver) {
	_p_server = pserver;
}


void LogicSystem::DealMsg() {
	for (;;) {
		std::unique_lock<std::mutex> unique_lk(_mutex);
		while (_msg_que.empty() && !_b_stop) {
			_consume.wait(unique_lk);
		}

		if (_b_stop ) {
			while (!_msg_que.empty()) {
				auto msg_node = _msg_que.front();
				cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
				auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
				if (call_back_iter == _fun_callbacks.end()) {
					_msg_que.pop();
					continue;
				}
				call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
					std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
				_msg_que.pop();
			}
			break;
		}

		auto msg_node = _msg_que.front();
		cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
		auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
		if (call_back_iter == _fun_callbacks.end()) {
			_msg_que.pop();
			std::cout << "msg id [" << msg_node->_recvnode->_msg_id << "] handler not found" << std::endl;
			continue;
		}
		call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id, 
			std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
		_msg_que.pop();
	}
}

void LogicSystem::RegisterCallBacks() {
	_fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::LoginHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_SEARCH_USER_REQ] = std::bind(&LogicSystem::SearchInfo, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_ADD_FRIEND_REQ] = std::bind(&LogicSystem::AddFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_TEXT_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatTextMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_HEART_BEAT_REQ] = std::bind(&LogicSystem::HeartBeatHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	
}

void LogicSystem::LoginHandler(shared_ptr<CSession> session, const short &msg_id, const string &msg_data) {
	json root = json::parse(msg_data, /*callback=*/nullptr, /*allow_exceptions=*/false);
	json rtvalue; // 最终返回给客户端的 JSON
	Defer defer([&rtvalue, session]() {
        std::string return_str = rtvalue.dump();   // 如需可读性可用 dump(2)
        session->Send(return_str, MSG_CHAT_LOGIN_RSP);
    });

    // 基本健壮性：解析失败或字段缺失 => 直接返回错误
    if (root.is_discarded() || !root.is_object()) {
        rtvalue["error"] = ErrorCodes::UidInvalid; // 若有 ParamInvalid 更合适可替换
        return;
    }
    int uid = root.value("uid", 0);
    std::string token = root.value("token", std::string{});
    if (uid == 0 || token.empty()) {
        rtvalue["error"] = ErrorCodes::UidInvalid;
        return;
    }

    std::cout << "user login uid is " << uid
              << " user token is " << token << std::endl;

	// 校验 token（Redis）
	const std::string uid_str   = std::to_string(uid);
    const std::string token_key = USERTOKENPREFIX + uid_str;

    std::string token_value;
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (!success) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return ;
	}

	if (token_value != token) {
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		return ;
	}

	rtvalue["error"] = ErrorCodes::Success;

	// 用户基础信息：先 Redis，失败回源 MySQL
	std::string base_key = USER_BASE_INFO + uid_str;
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = GetBaseInfo(base_key, uid, user_info);
	if (!b_base) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}
	// 基础信息填充
    rtvalue["uid"]   = uid;
    rtvalue["pwd"]   = user_info->pwd;
    rtvalue["name"]  = user_info->name;
    rtvalue["email"] = user_info->email;
    rtvalue["nick"]  = user_info->nick;
    rtvalue["desc"]  = user_info->desc;
    rtvalue["sex"]   = user_info->sex;
    rtvalue["icon"]  = user_info->icon;

	// 读取好友申请列表
    std::vector<std::shared_ptr<ApplyInfo>> apply_list;
    if (GetFriendApplyInfo(uid, apply_list)) {
        json apply_arr = json::array();
        for (const auto& apply : apply_list) {
            apply_arr.push_back({
                {"name",   apply->_name},
                {"uid",    apply->_uid},
                {"icon",   apply->_icon},
                {"nick",   apply->_nick},
                {"sex",    apply->_sex},
                {"desc",   apply->_desc},
                {"status", apply->_status}
            });
        }
        rtvalue["apply_list"] = std::move(apply_arr);    
    }

	// 读取好友列表
    std::vector<std::shared_ptr<UserInfo>> friend_list;
    if (GetFriendList(uid, friend_list)) {
		json friend_arr = json::array();
		for (const auto& f : friend_list) {
			friend_arr.push_back({
				{"name",  f->name},
				{"uid",   f->uid},
				{"icon",  f->icon},
				{"nick",  f->nick},
				{"sex",   f->sex},
				{"desc",  f->desc},
				{"back",  f->back}
			});
		}
		rtvalue["friend_list"] = std::move(friend_arr);
    }

	// 分布式踢下线 + 绑定会话
	auto server_name = ConfigMgr::Inst().GetValue("SelfServer", "Name");
    {
        // 分布式锁，避免并发登录竞态
        const auto lock_key   = LOCK_PREFIX + uid_str;
        const auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
        Defer defer2([identifier, lock_key]() {
            RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
        });

		// 检测旧登录
        std::string uid_ip_value;
        const auto uid_ip_key = USERIPPREFIX + uid_str;
        bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);

		if (b_ip) {
            // 已经在某台服务器登录
            auto& cfg = ConfigMgr::Inst();
            auto self_name = cfg["SelfServer"]["Name"];

            if (uid_ip_value == self_name) {
                // 就在本机：踢旧连接
                auto old_session = UserMgr::GetInstance()->GetSession(uid);
                if (old_session) {
                    old_session->NotifyOffline(uid);
                    _p_server->ClearSession(old_session->GetSessionId());
                }
            } else {
                // 不在本机：通过 gRPC 通知远端踢下线
                KickUserReq kick_req;
                kick_req.set_uid(uid);
                ChatGrpcClient::GetInstance()->NotifyKickUser(uid_ip_value, kick_req);
            }
        }

		// 绑定当前会话
        session->SetUserId(uid);
        RedisMgr::GetInstance()->Set(uid_ip_key, server_name);
        UserMgr::GetInstance()->SetUserSession(uid, session);

        const std::string uid_session_key = USER_SESSION_PREFIX + uid_str;
        RedisMgr::GetInstance()->Set(uid_session_key, session->GetSessionId());
    }

	return;
}

void LogicSystem::SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
    // 1) 解析输入（非抛异常）
    json root = json::parse(msg_data, /*callback=*/nullptr, /*allow_exceptions=*/false);

    json rtvalue;
    Defer defer([&rtvalue, session]() {
        std::string return_str = rtvalue.dump(); // 调试期可用 dump(2)
        session->Send(return_str, ID_SEARCH_USER_RSP);
    });

    if (root.is_discarded() || !root.is_object()) {
        rtvalue["error"] = ErrorCodes::UidInvalid; // 或者 ParamInvalid
        return;
    }

    std::string uid_str = root.value("uid", std::string{});
    std::cout << "user SearchInfo uid is " << uid_str << std::endl;

    // 2) 分支调用
    if (isPureDigit(uid_str)) {
        // 注意：把 GetUserByUid 的签名改成：GetUserByUid(const std::string&, nlohmann::json& out)
        GetUserByUid(uid_str, rtvalue);
    } else {
        // 同理：GetUserByName(const std::string&, nlohmann::json& out)
        GetUserByName(uid_str, rtvalue);
    }
    return;
}

void LogicSystem::AddFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	// 1) 解析输入（非抛异常）
    json root = json::parse(msg_data, /*callback=*/nullptr, /*allow_exceptions=*/false);

    json rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    Defer defer([&rtvalue, session]() {
        std::string return_str = rtvalue.dump();
        session->Send(return_str, ID_ADD_FRIEND_RSP);
    });

    if (root.is_discarded() || !root.is_object()) {
        rtvalue["error"] = ErrorCodes::UidInvalid; // 或 ParamInvalid
        return;
    }

    int uid                 = root.value("uid", 0);
    std::string applyname   = root.value("applyname", std::string{});
    std::string bakname     = root.value("bakname", std::string{}); // 你当前未使用
    int touid               = root.value("touid", 0);

    std::cout << "user login uid is " << uid
              << " applyname is " << applyname
              << " bakname is "   << bakname
              << " touid is "     << touid << std::endl;

    if (uid == 0 || touid == 0 || applyname.empty()) {
        rtvalue["error"] = ErrorCodes::UidInvalid; // 或 ParamInvalid
        return;
    }

	// 2) 先写数据库
    MysqlMgr::GetInstance()->AddFriendApply(uid, touid);

    // 3) 查询对端所在服务器
    const std::string to_str    = std::to_string(touid);
    const std::string to_ip_key = USERIPPREFIX + to_str;
    std::string to_ip_value;
    bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
    if (!b_ip) {
        // 未找到就直接返回（rtvalue 仍是 Success）
        return;
    }

    auto& cfg        = ConfigMgr::Inst();
    auto self_name   = cfg["SelfServer"]["Name"];


	// 4) 查发起者的基础信息（用于通知 payload）
    const std::string base_key = USER_BASE_INFO + std::to_string(uid);
    auto apply_info = std::make_shared<UserInfo>();
    bool b_info = GetBaseInfo(base_key, uid, apply_info);

	// 5) 在本机：直接推送
    if (to_ip_value == self_name) {
        auto peer_session = UserMgr::GetInstance()->GetSession(touid); // 避免遮蔽入参 session
        if (peer_session) {
            json notify = {
                {"error",    ErrorCodes::Success},
                {"applyuid", uid},
                {"name",     applyname},
                {"desc",     ""} // 和你原逻辑一致
            };
            if (b_info) {
                notify["icon"] = apply_info->icon;
                notify["sex"]  = apply_info->sex;
                notify["nick"] = apply_info->nick;
            }
            peer_session->Send(notify.dump(), ID_NOTIFY_ADD_FRIEND_REQ);
        }
        return;
	}

	
	// 6) 不在本机：通过 gRPC 通知对端
    AddFriendReq add_req;
    add_req.set_applyuid(uid);
    add_req.set_touid(touid);
    add_req.set_name(applyname);
    add_req.set_desc(""); // 原逻辑
    if (b_info) {
        add_req.set_icon(apply_info->icon);
        add_req.set_sex(apply_info->sex);
        add_req.set_nick(apply_info->nick);
    }
    ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value, add_req);

}

void LogicSystem::AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	
	// 解析输入（非抛异常）
    json root = json::parse(msg_data, /*callback=*/nullptr, /*allow_exceptions=*/false);

    // 应答体
    json rtvalue;
    rtvalue["error"] = ErrorCodes::Success;

    // 统一应答发送
    Defer defer([&rtvalue, session]() {
        std::string return_str = rtvalue.dump();   // 生产用 dump()；调试可 dump(2)
        session->Send(return_str, ID_AUTH_FRIEND_RSP);
    });

    if (root.is_discarded() || !root.is_object()) {
        rtvalue["error"] = ErrorCodes::UidInvalid; // 或 ParamInvalid
        return;
    }

    int uid      = root.value("fromuid", 0);
    int touid    = root.value("touid", 0);
    std::string back_name = root.value("back", std::string{});

    std::cout << "from " << uid << " auth friend to " << touid << std::endl;

    // 查询对端（被添加者）基本信息，填充应答
    auto user_info = std::make_shared<UserInfo>();
    const std::string base_key = USER_BASE_INFO + std::to_string(touid);
    bool b_info = GetBaseInfo(base_key, touid, user_info);
    if (b_info) {
        rtvalue["name"] = user_info->name;
        rtvalue["nick"] = user_info->nick;
        rtvalue["icon"] = user_info->icon;
        rtvalue["sex"]  = user_info->sex;
        rtvalue["uid"]  = touid;
    } else {
        rtvalue["error"] = ErrorCodes::UidInvalid;
    }

    // 先更新数据库（同原逻辑）
	MysqlMgr::GetInstance()->AuthFriendApply(uid, touid);
	MysqlMgr::GetInstance()->AddFriend(uid, touid,back_name);

	// 查询对端所在服务器
    const std::string to_ip_key = USERIPPREFIX + std::to_string(touid);
    std::string to_ip_value;
    bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
    if (!b_ip) {
        return; // 找不到在线位置
    }

	auto& cfg      = ConfigMgr::Inst();
    auto self_name = cfg["SelfServer"]["Name"];
	// 就在本机：直接通知对端
    if (to_ip_value == self_name) {
        auto peer_session = UserMgr::GetInstance()->GetSession(touid); // 避免遮蔽形参 session
        if (peer_session) {
            json notify = {
                {"error",   ErrorCodes::Success},
                {"fromuid", uid},
                {"touid",   touid},
            };

            // 补充 fromuid 的基本信息
            const std::string base_key2 = USER_BASE_INFO + std::to_string(uid);
            auto from_info = std::make_shared<UserInfo>();
            bool b_info2 = GetBaseInfo(base_key2, uid, from_info);
            if (b_info2) {
                notify["name"] = from_info->name;
                notify["nick"] = from_info->nick;
                notify["icon"] = from_info->icon;
                notify["sex"]  = from_info->sex;
            } else {
                notify["error"] = ErrorCodes::UidInvalid;
            }

            peer_session->Send(notify.dump(), ID_NOTIFY_AUTH_FRIEND_REQ);
        }
        return;
	}


	// 不在本机：通过 gRPC 通知对端
    AuthFriendReq auth_req;
    auth_req.set_fromuid(uid);
    auth_req.set_touid(touid);
    ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
}

void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	// 解析输入（非抛异常）
    json root = json::parse(msg_data, /*callback=*/nullptr, /*allow_exceptions=*/false);

    // 统一应答对象
    json rtvalue;
    rtvalue["error"] = ErrorCodes::Success;

    Defer defer([&rtvalue, session]() {
        std::string return_str = rtvalue.dump();
        session->Send(return_str, ID_TEXT_CHAT_MSG_RSP);
    });

    if (root.is_discarded() || !root.is_object()) {
        rtvalue["error"] = ErrorCodes::UidInvalid; // 或 ParamInvalid
        return;
    }

    int uid   = root.value("fromuid", 0);
    int touid = root.value("touid", 0);

    // text_array：容错处理（缺失或类型不对时给空数组）
    json arrays = json::array();
    if (root.contains("text_array") && root["text_array"].is_array()) {
        arrays = root["text_array"];
    }

    // 填充应答
    rtvalue["fromuid"]    = uid;
    rtvalue["touid"]      = touid;
    rtvalue["text_array"] = arrays;


	// 查询对端所在服务器
    const std::string to_ip_key = USERIPPREFIX + std::to_string(touid);
    std::string to_ip_value;
    bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
    if (!b_ip) {
        return;
    }

    auto& cfg      = ConfigMgr::Inst();
    auto self_name = cfg["SelfServer"]["Name"];
	// 本机：直接下发通知
    if (to_ip_value == self_name) {
        auto peer_session = UserMgr::GetInstance()->GetSession(touid);
        if (peer_session) {
            peer_session->Send(rtvalue.dump(), ID_NOTIFY_TEXT_CHAT_MSG_REQ);
        }
        return;
    }

	// 跨机：组织 gRPC 请求
    TextChatMsgReq text_msg_req;
    text_msg_req.set_fromuid(uid);
    text_msg_req.set_touid(touid);
	// 逐条拷贝消息
    if (arrays.is_array()) {
        for (const auto& txt_obj : arrays) {
            // 容错：缺字段给默认值
            std::string content = txt_obj.value("content", std::string{});
            std::string msgid   = txt_obj.value("msgid", std::string{});

            std::cout << "content is " << content << std::endl;
            std::cout << "msgid is "   << msgid   << std::endl;

            auto* text_msg = text_msg_req.add_textmsgs();
            text_msg->set_msgid(msgid);
            text_msg->set_msgcontent(content);
        }
    }


	// 调用对端
    ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
}

void LogicSystem::HeartBeatHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	json root = json::parse(msg_data, /*callback=*/nullptr, /*allow_exceptions=*/false);
    int uid = 0;
    if (!root.is_discarded() && root.is_object()) {
        uid = root.value("fromuid", 0);
    }
    std::cout << "receive heart beat msg, uid is " << uid << std::endl;

    json rtvalue = {
        {"error", ErrorCodes::Success}
    };
    session->Send(rtvalue.dump(), ID_HEARTBEAT_RSP);
}

bool LogicSystem::isPureDigit(const std::string& str)
{
	if (str.empty()) return false;
	for (unsigned char c : str) {
		if (!std::isdigit(c)) {
			return false;
		}
	}
	return true;
}

void LogicSystem::GetUserByUid(std::string uid_str, json& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = USER_BASE_INFO + uid_str;

	// 先查 Redis
	std::string info_str;
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
        json root = json::parse(info_str, /*callback=*/nullptr, /*allow_exceptions=*/false);
        if (!root.is_discarded() && root.is_object()) {
            int         uid   = root.value("uid",   0);
            std::string name  = root.value("name",  std::string{});
            std::string pwd   = root.value("pwd",   std::string{});
            std::string email = root.value("email", std::string{});
            std::string nick  = root.value("nick",  std::string{});
            std::string desc  = root.value("desc",  std::string{});
            int         sex   = root.value("sex",   0);
            std::string icon  = root.value("icon",  std::string{});

            std::cout << "user uid is " << uid << " name is "
                      << name << " pwd is " << pwd << " email is "
                      << email << " icon is " << icon << std::endl;

            rtvalue["uid"]   = uid;
            rtvalue["pwd"]   = pwd;
            rtvalue["name"]  = name;
            rtvalue["email"] = email;
            rtvalue["nick"]  = nick;
            rtvalue["desc"]  = desc;
            rtvalue["sex"]   = sex;
            rtvalue["icon"]  = icon;
            return;
        }
        // 解析失败则回源 DB
    }

	// Redis 未命中或损坏：查 DB
    int uid = 0;
    try {
        uid = std::stoi(uid_str);
    } catch (...) {
        rtvalue["error"] = ErrorCodes::UidInvalid;
        return;
    }

    std::shared_ptr<UserInfo> user_info = MysqlMgr::GetInstance()->GetUser(uid);
    if (!user_info) {
        rtvalue["error"] = ErrorCodes::UidInvalid;
        return;
    }

	// 写回 Redis
    json redis_root = {
        {"uid",   user_info->uid},
        {"pwd",   user_info->pwd},
        {"name",  user_info->name},
        {"email", user_info->email},
        {"nick",  user_info->nick},
        {"desc",  user_info->desc},
        {"sex",   user_info->sex},
        {"icon",  user_info->icon}
    };
    RedisMgr::GetInstance()->Set(base_key, redis_root.dump());

	// 返回
    rtvalue["uid"]   = user_info->uid;
    rtvalue["pwd"]   = user_info->pwd;
    rtvalue["name"]  = user_info->name;
    rtvalue["email"] = user_info->email;
    rtvalue["nick"]  = user_info->nick;
    rtvalue["desc"]  = user_info->desc;
    rtvalue["sex"]   = user_info->sex;
    rtvalue["icon"]  = user_info->icon;
}

void LogicSystem::GetUserByName(std::string name, json& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = NAME_INFO + name;

	// 先查 Redis
    std::string info_str;
    bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
    if (b_base) {
        json root = json::parse(info_str, /*callback=*/nullptr, /*allow_exceptions=*/false);
        if (!root.is_discarded() && root.is_object()) {
            int         uid   = root.value("uid",   0);
            std::string uname = root.value("name",  std::string{});
            std::string pwd   = root.value("pwd",   std::string{});
            std::string email = root.value("email", std::string{});
            std::string nick  = root.value("nick",  std::string{});
            std::string desc  = root.value("desc",  std::string{});
            int         sex   = root.value("sex",   0);
            // 注意：你原实现这里没有 icon 字段，保持一致；若需要也可加上

            std::cout << "user uid is " << uid << " name is "
                      << uname << " pwd is " << pwd << " email is "
                      << email << std::endl;

			rtvalue["uid"]   = uid;
            rtvalue["pwd"]   = pwd;
            rtvalue["name"]  = uname;
            rtvalue["email"] = email;
            rtvalue["nick"]  = nick;
            rtvalue["desc"]  = desc;
            rtvalue["sex"]   = sex;
            return;
		}
		// 解析失败则回源 DB
	}

	// Redis 未命中：查 DB
    std::shared_ptr<UserInfo> user_info = MysqlMgr::GetInstance()->GetUser(name);
    if (!user_info) {
        rtvalue["error"] = ErrorCodes::UidInvalid;
        return;
    }

	// 写回 Redis（保持与你原实现一致：不写 icon）
    json redis_root = {
        {"uid",   user_info->uid},
        {"pwd",   user_info->pwd},
        {"name",  user_info->name},
        {"email", user_info->email},
        {"nick",  user_info->nick},
        {"desc",  user_info->desc},
        {"sex",   user_info->sex}
    };
    RedisMgr::GetInstance()->Set(base_key, redis_root.dump());

    // 返回
    rtvalue["uid"]   = user_info->uid;
    rtvalue["pwd"]   = user_info->pwd;
    rtvalue["name"]  = user_info->name;
    rtvalue["email"] = user_info->email;
    rtvalue["nick"]  = user_info->nick;
    rtvalue["desc"]  = user_info->desc;
    rtvalue["sex"]   = user_info->sex;
}

bool LogicSystem::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
{
	// 先查 Redis
	std::string info_str;
    bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
    if (b_base) {
        json root = json::parse(info_str, /*callback=*/nullptr, /*allow_exceptions=*/false);
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
                      << " pwd is "  << userinfo->pwd
                      << " email is " << userinfo->email << std::endl;
            return true;
        }
        // Redis 命中但 JSON 异常：继续回源 DB
    }

    // 回源 DB
    std::shared_ptr<UserInfo> user_info = MysqlMgr::GetInstance()->GetUser(uid);
    if (!user_info) {
        return false;
    }
    userinfo = user_info;

    // 写回 Redis
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
    RedisMgr::GetInstance()->Set(base_key, redis_root.dump());

    return true;
}

bool LogicSystem::GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>> &list) {
	return MysqlMgr::GetInstance()->GetApplyList(to_uid, list, 0, 10);
}

bool LogicSystem::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list) {
	return MysqlMgr::GetInstance()->GetFriendList(self_id, user_list);
}
