#include "chatserver.hpp"
#include "chatservice.hpp"
#include "json.hpp"

#include <functional>
#include <string>

using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg):_server(loop, listenAddr, nameArg), _loop(loop)
            {
                _server.setConnectCallback(bind(&ChatServer::onConnection, this, _1));

                _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));

                _server.setThreadNum(4);
            }

void ChatServer::start(){
    _server.start();
}            

void ChatServer::onConnection(const TcpConnectionPtr &conn){

    if(!conn->connected()){ // 客户端断开连接。muduo的日志模块也会打印
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp time){
    string buf = buffer->retrieveAllAsString();
    json js = json::parse(buf); // 数据的反序列化
    //通过js["msgid"] 获取 -> 业务handler -> conn js time
    // 达到的目的，完全解耦网络模块的代码和业务模块的代码
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>()); // 获得事件处理器
    msgHandler(conn, js, time); // 回调消息绑定好事件处理器，来执行相应的业务处理
}