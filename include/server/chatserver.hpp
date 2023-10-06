#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
using namespace muduo;
using namespace muduo::net;


//聊天服务器的主类
class ChatServer
{
private:
  TcpServer _server; // 组合的muduo库，实现服务器功能的类对象
  EventLoop *_loop; // 指向事件循环对象的指针

  void onConnection(const TcpConnectionPtr&);

  void onMessage(const TcpConnectionPtr&, Buffer*, Timestamp);
public:
  ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg);
  void start();
};

#endif