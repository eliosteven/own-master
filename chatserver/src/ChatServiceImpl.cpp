#include "ChatServiceImpl.h"
#include "UserMgr.h"
#include "CSession.h"
#include <nlohmann/json.hpp>
#include "RedisMgr.h"
#include "MysqlMgr.h"

ChatServiceImpl::ChatServiceImpl()
{

}

Status ChatServiceImpl::NotifyAddFriend(ServerContext* context, const AddFriendReq* request, AddFriendRsp* reply)
{
	// 判断用户是否在本服务器
	auto touid = request->touid();
	auto session = UserMgr::GetInstance()->GetSession(touid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_applyuid(request->applyuid());
		reply->set_touid(request->touid());
		});

	// 用户不在本服务器，直接返回
	if (session == nullptr) {
		return Status::OK;
	}
	
	// 在本服务器，直接下发通知
	json rtvalue = {
        {"error",    ErrorCodes::Success},
        {"applyuid", request->applyuid()},
        {"name",     request->name()},
        {"desc",     request->desc()},
        {"icon",     request->icon()},
        {"sex",      request->sex()},
        {"nick",     request->nick()},
    };


	std::string return_str = rtvalue.dump();
    session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
    return Status::OK;
}

Status ChatServiceImpl::NotifyAuthFriend(ServerContext* context,
                                         const AuthFriendReq* request,
                                         AuthFriendRsp* reply)
{
	auto touid = request->touid();
	auto fromuid = request->fromuid();
	auto session = UserMgr::GetInstance()->GetSession(touid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_fromuid(request->fromuid());
		reply->set_touid(request->touid());
		});

	// 用户不在本服务器，直接返回
	if (session == nullptr) {
		return Status::OK;
	}

	// 在本服务器，直接下发通知
    json rtvalue = {
        {"error",  ErrorCodes::Success},
        {"fromuid", request->fromuid()},
        {"touid",   request->touid()},
    };

	std::string base_key = USER_BASE_INFO + std::to_string(fromuid);
	auto user_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, fromuid, user_info);
	if (b_info) {
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
	}
	else {
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}

	std::string return_str = rtvalue.dump();
    session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
    return Status::OK;
}

Status ChatServiceImpl::NotifyTextChatMsg(::grpc::ServerContext* context,
                                          const TextChatMsgReq* request,
                                          TextChatMsgRsp* reply)
{
	// 判断用户是否在本服务器
	auto touid = request->touid();
	auto session = UserMgr::GetInstance()->GetSession(touid);
	reply->set_error(ErrorCodes::Success);

	// 用户不在本服务器，直接返回
	if (session == nullptr) {
		return Status::OK;
	}

	// 在本服务器，组织 JSON 下发
    json rtvalue = {
        {"error",   ErrorCodes::Success},
        {"fromuid", request->fromuid()},
        {"touid",   request->touid()},
    };

    // 把多条文本消息组织为数组
    json text_array = json::array();
    for (const auto& msg : request->textmsgs()) {
        text_array.push_back({
            {"content", msg.msgcontent()},
            {"msgid",   msg.msgid()}
        });
    }
    rtvalue["text_array"] = std::move(text_array);

	std::string return_str = rtvalue.dump();
    session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
    return Status::OK;
}


bool ChatServiceImpl::GetBaseInfo(std::string base_key,
                                  int uid,
                                  std::shared_ptr<UserInfo>& userinfo)
{
    // 先查 redis
    std::string info_str;
    bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
    if (b_base) {
        // 非抛异常解析；失败则回源 DB
        json root = json::parse(info_str, /*callback=*/nullptr, /*allow_exceptions=*/false);
        if (!root.is_discarded() && root.is_object()) {
            if (!userinfo) {
                userinfo = std::make_shared<UserInfo>();
            }
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
                      << " pwd is "   << userinfo->pwd
                      << " email is " << userinfo->email
                      << std::endl;

            return true;
        }
        // 解析失败，继续回源
    }

    // redis 未命中或解析失败：查 mysql
    std::shared_ptr<UserInfo> user_info = MysqlMgr::GetInstance()->GetUser(uid);
    if (!user_info) {
        return false;
    }
    userinfo = user_info;

    // 写回 redis
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

Status ChatServiceImpl::NotifyKickUser(::grpc::ServerContext* context,
                                       const KickUserReq* request,
                                       KickUserRsp* reply)
{
    // 判断用户是否在本服务器
    auto uid = request->uid();
    auto session = UserMgr::GetInstance()->GetSession(uid);

    Defer defer([request, reply]() {
        reply->set_error(ErrorCodes::Success);
        reply->set_uid(request->uid());
    });

    // 用户不在本服务器，直接返回
    if (session == nullptr) {
        return Status::OK;
    }

    // 在本服务器，通知对端并清理会话
    session->NotifyOffline(uid);
    _p_server->ClearSession(session->GetSessionId());

    return Status::OK;
}

void ChatServiceImpl::RegisterServer(std::shared_ptr<CServer> pServer)
{
	_p_server = pServer;
}
