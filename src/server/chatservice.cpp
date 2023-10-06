#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/logging.h>
#include <vector>
#include <map>

using namespace std;
using namespace muduo;

//获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的handler回调操作
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调
    _msgHandlerMap.insert(LOGIN_MSG, bind(&ChatService::login, this, _1, _2, _3));
    _msgHandlerMap.insert(REG_MSG, bind(&ChatService::reg, this, _1, _2, _3));
    _msgHandlerMap.insert(ONE_CHAT_MSG, bind(&ChatService::oneChat, this, _1, _2, _3));
    _msgHandlerMap.insert(ADD_FRIEND_MSG, bind(&ChatService::addFriend, this, _1, _2, _3));
    _msgHandlerMap.insert(LOGINOUT_MSG, bind(&ChatService::loginout, this, _1, _2, _3));

     // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

MsgHandler ChatService::getHandler(int msgId)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgId);
    if(it == _msgHandlerMap.end()){

        //LOG_ERROR << "msgid:" << msgId  <<  "can not find handler";
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, Json& js, Timestamp time) {
            LOG_ERROR << "msgid:" << msgId  <<  "can not find handler";
        }

    }
    else {
        return _msgHandlerMap[msgId];
    }
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, Json& js, Timestamp time)
{
    LOG_INFO << "do login service!!!";
    int id = js["id"].get<int>();
    string password = js["password"];

    User user = _userModel.query(id);

    json response;
    response["msgid"] = LOGIN_MSG_ACK;

    if(user.getId() == id && user.getPwd() == password) {

        if(user.getState() == "online"){
            //已登录，不允许重复登录
        response["errno"] = 2; // 标识
        response["errmsg"] = "this account is using, input another!";
        conn->send(response.dump());
        return;

        }

        //登录成功,记录用户连接信息

        // 加一个作用域，可以减小锁的粒度
        {
           lock_guard<mutex> lock(_connMutex);
           _userConnMap.insert({id, conn});
        }
        // id用户登录成功后，向redis订阅channel(id)
        _redis.subscribe(id); 

        user.setState("online");
        _userModel.updateState(user);

        response["errno"] = 0; // 标识
        response["id"] = user.getId();
        response["name"] = user.getName();

        // 查询该用户是否有离线消息
        vector<string> vec = _offlinemsgModel.query(id);

        if(!vec.empty()){
            response["offlinemsg"] = vec;
            //读取该用户的离线消息后, 把该用户的所有离线消息删除掉
            _offlinemsgModel.remove(id);
        }

        // 查询该用户的好友信息并返回
        vector<User> friends = _friendModel.query(id);
        if(!friends.empty()){
            vector<string> vec2;
            for(auto user : friends){
                json js;
                js["id"] = user.getId();
                js["name"] = user.getName();
                js["state"] = user.getState();
                vec2.push_back(js.dump());
            }
            response["friends"] = vec2;
        }

         // 查询用户的群组信息
        vector<Group> groupuserVec = _groupModel.queryGroups(id);
        if (!groupuserVec.empty())
        {
             // group:[{groupid:[xxx, xxx, xxx, xxx]}]
            vector<string> groupV;
            for (Group &group : groupuserVec)
            {
                json grpjson;
                grpjson["id"] = group.getId();
                grpjson["groupname"] = group.getName();
                grpjson["groupdesc"] = group.getDesc();
                vector<string> userV;
                for (GroupUser &user : group.getUsers())
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    js["role"] = user.getRole();
                    userV.push_back(js.dump());
                }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }

                response["groups"] = groupV;
            }
    } else {
        response["errno"] = 1; // 标识
        response["errmsg"] = "id or password is invalid!";
    }

    conn->send(response.dump());
    return;
}

// 处理注册业务  name  password   业务操作的都是数据对象
void ChatService::reg(const TcpConnectionPtr &conn, Json& js, Timestamp time)
{
    LOG_INFO << "do reg service!!!";

    string name = js["name"];
    string password = js["password"];

    User user;
    user.setName(name);
    user.setPwd(password);
    bool state = _userModel.insert(user);
    if(state) {
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0; // 标识
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1; // 标识
        conn->send(response.dump());
    }

}

// 处理客户的异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutext> lock(_connMutex);
        for(auto iter = _userConnMap.begin(); iter != _userConnMap.end(); iter++){
            if(iter->second == conn){
                // 从Map表删除用户的连接信息
                user.setId(iter->first);
                _userConnMap.erase(iter);
                break;
            }
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId()); 

    if(user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
    return;
}

//处理用户注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, Json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto iter = _userConnMap.find(userid);
        if(iter != _userConnMap.end()){
            _userConnMap.erase(iter);
        }
    }  

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid); 

    User user;
    user.setId(userid);
    user.setState("offline");
    _userModel.updateState(user);
    return;
}


// 一对一聊天业务  msgid  id  name  to
void ChatService::oneChat(const TcpConnectionPtr &conn, Json& js, Timestamp time)
{
    int toId = js["to"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto iter = _userConnMap.find(toId);
        if(iter != _userConnMap.end())
        {
            // toId在线,转发消息。不能放在外面，因为可能在转发消息时，对方下线
            // 服务器主动推送消息给toId用户
            it->second->send(js.dump());
            return;
        }
    }

    // 查询toid是否在线 
    User user = _userModel.query(toId);
    if (user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }

    // toId不在线，存储离线消息
    _offlinemsgModel.insert(toId, js.dump());

    return;
}

void ChatService::reset()
{
    //把所有online状态的用户，设置成offline
    _userModel.resetState();
    return; 
}

// msgid  id  friendid  添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn, Json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["id"].get<int>();

    //存储好友信息
    _friendModel.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询toid是否在线 
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 存储离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}