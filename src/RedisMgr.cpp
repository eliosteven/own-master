#include "RedisMgr.h"
#include "ConfigMgr.h"

RedisMgr::RedisMgr() {
    auto& gCfgMgr = ConfigMgr::Inst();
    auto host = gCfgMgr["Redis"]["Host"];
    auto port = gCfgMgr["Redis"]["Port"];
    auto pwd = gCfgMgr["Redis"]["Passwd"];
    _con_pool.reset(new RedisConPool(5, host.c_str(), atoi(port.c_str()), pwd.c_str()));
}

RedisMgr::~RedisMgr() {
    Close();  //防止资源泄露
}

bool RedisMgr::Get(const std::string& key, std::string& value) {
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    RedisReplyWrapper reply((redisReply*)redisCommand(connect, "GET %s", key.c_str()));
    if (!reply.get()) {
        std::cerr << "[GET " << key << "] failed: NULL reply" << std::endl;
        return false;
    }

    if (reply->type != REDIS_REPLY_STRING) {
        std::cerr << "[GET " << key << "] failed: type = " << reply->type << std::endl;
        return false;
    }

    value = reply->str;
    std::cout << "GET [" << key << "] = " << value << std::endl;
    return true;
}

bool RedisMgr::Set(const std::string &key, const std::string &value) {
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    RedisReplyWrapper reply((redisReply*)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str()));

    if (!reply.get()) {
        std::cerr << "Execute command [ SET " << key << " " << value << " ] failed: null reply" << std::endl;
        return false;
    }

    if (!(reply->type == REDIS_REPLY_STATUS && 
         (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0))) {
        std::cerr << "Execute command [ SET " << key << " " << value << " ] failed: reply = " 
                  << (reply->str ? reply->str : "(null)") << std::endl;
        return false;
    }

    std::cout << "Execute command [ SET " << key << " " << value << " ] success!" << std::endl;
    return true;
}

bool RedisMgr::Auth(const std::string &password)
{
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    RedisReplyWrapper reply((redisReply*)redisCommand(connect, "AUTH %s", password.c_str()));
    if (!reply.get()) {
        std::cerr << "Execute command [AUTH " << password << "] failed: reply is null\n" << std::endl;
        return false;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        std::cout << "认证失败" << std::endl;
        return false;
    }
    else {
        std::cout << "认证成功" << std::endl;
        return true;
    }
}

bool RedisMgr::LPush(const std::string& key, const std::string& value) {
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    RedisReplyWrapper reply((redisReply*)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str()));
    if (!reply.get()) {
        std::cerr << "Execute command [LPUSH " << key << " " << value << "] failed: null reply\n";
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        std::cerr << "Execute command [LPUSH " << key << " " << value << "] failed: not an integer reply or <= 0\n";
        return false;
    }

    std::cout << "Execute command [LPUSH " << key << " " << value << "] success!\n";
    return true;
}

bool RedisMgr::RPush(const std::string& key, const std::string& value) {
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    RedisReplyWrapper reply((redisReply*)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str()));
    if (!reply.get()) {
        std::cerr << "Execute command [RPUSH " << key << " " << value << "] failed: null reply\n";
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        std::cerr << "Execute command [RPUSH " << key << " " << value << "] failed: not an integer reply or <= 0\n";
        return false;
    }

    std::cout << "Execute command [RPUSH " << key << " " << value << "] success!\n";
    return true;
}

bool RedisMgr::LPop(const std::string& key, std::string& value) {
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    RedisReplyWrapper reply((redisReply*)redisCommand(connect, "LPOP %s", key.c_str()));
    if (!reply.get() || reply->type == REDIS_REPLY_NIL) {
        std::cerr << "Execute command [LPOP " << key << "] failed: reply is nil or null\n";
        return false;
    }

    value = reply->str;
    std::cout << "Execute command [LPOP " << key << "] success! value: " << value << "\n";
    return true;
}

bool RedisMgr::RPop(const std::string& key, std::string& value) {
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    RedisReplyWrapper reply((redisReply*)redisCommand(connect, "RPOP %s", key.c_str()));
    if (!reply.get() || reply->type == REDIS_REPLY_NIL) {
        std::cerr << "Execute command [RPOP " << key << "] failed: reply is nil or null\n";
        return false;
    }

    value = reply->str;
    std::cout << "Execute command [RPOP " << key << "] success! value: " << value << "\n";
    return true;
}

bool RedisMgr::HSet(const std::string &key, const std::string &hkey, const std::string &value) {
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    RedisReplyWrapper reply((redisReply*)redisCommand(connect, "HSET %s %s %s",
                                                      key.c_str(), hkey.c_str(), value.c_str()));
    if (!reply.get() || reply->type != REDIS_REPLY_INTEGER) {
        std::cerr << "Execute command [HSET " << key << " " << hkey << " " << value << "] failed.\n";
        return false;
    }

    std::cout << "Execute command [HSET " << key << " " << hkey << " " << value << "] success!\n";
    return true;
}

bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen) {
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    const char* argv[] = { "HSET", key, hkey, hvalue };
    size_t argvlen[]   = { 4, strlen(key), strlen(hkey), hvaluelen };

    RedisReplyWrapper reply((redisReply*)redisCommandArgv(connect, 4, argv, argvlen));
    if (!reply.get() || reply->type != REDIS_REPLY_INTEGER) {
        std::cerr << "Execute command [HSET " << key << " " << hkey << " <binary value> ] failed.\n";
        return false;
    }

    std::cout << "Execute command [HSET " << key << " " << hkey << " <binary value> ] success!\n";
    return true;
}

std::string RedisMgr::HGet(const std::string &key, const std::string &hkey)
{
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return "";
    }

    const char* argv[] = { "HGET", key.c_str(), hkey.c_str() };
    size_t argvlen[]   = { 4, key.length(), hkey.length() };

    RedisReplyWrapper reply((redisReply*)redisCommandArgv(connect, 3, argv, argvlen));

    if (!reply.get() || reply->type == REDIS_REPLY_NIL || reply->type != REDIS_REPLY_STRING) {
        std::cerr << "Execute command [HGET " << key << " " << hkey << "] failed!\n";
        return "";
    }

    std::cout << "Execute command [HGET " << key << " " << hkey << "] success!\n";
    return std::string(reply->str, reply->len);  // 注意 reply->str 可能是二进制字符串
}

bool RedisMgr::Del(const std::string &key)
{
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    RedisReplyWrapper reply((redisReply*)redisCommand(connect, "DEL %s", key.c_str()));

    if (!reply.get() || reply->type != REDIS_REPLY_INTEGER) {
        std::cerr << "Execute command [DEL " << key << "] failed or key not found!\n";
        return false;
    }

    std::cout << "Execute command [DEL " << key << "] success! Deleted " << reply->integer << " keys.\n";
    return true;
}

bool RedisMgr::ExistsKey(const std::string &key)
{
    RedisConnectionGuard conn(_con_pool.get());
    redisContext* connect = conn.get();
    if(connect == nullptr) {
        return false;
    }

    RedisReplyWrapper reply((redisReply*)redisCommand(connect, "EXISTS %s", key.c_str()));

    if (!reply.get() || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        std::cerr << "Execute command [EXISTS " << key << "] failed!\n";
        return false;
    }

    std::cout << "Key [" << key << "] exists.\n";
    return true;
}

void RedisMgr::Close() {
    _con_pool->Close();
}

