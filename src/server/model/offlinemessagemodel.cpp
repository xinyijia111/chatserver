#include "offlinemessagemodel.hpp"
#include "db.h"

 //存储用户的离线消息
    void OfflineMsgModel::insert(int userid, string msg)
    {
        // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert  offlinemessage values(%d, '%s')", userid, msg.c_str());

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
    }

    //删除用户的离线消息
    void OfflineMsgModel::remove(int userid)
    {
        char sql[1024] = {0};
        sprintf(sql, "delete offlinemessage where id = %d", userid);

        MySQL mysql;
        if(mysql.connect())
        {
           mysql.update(sql);
        }
    }
    

    //查询用户的离线消息
    vector<string> OfflineMsgModel::query(int userid)
    {
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where id = %d", userid);

    MySQL mysql;
    vector<string> vec;
    if(mysql.connect()){

        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr) {
            while(row = mysql_fetch_row(res) != nullptr){
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }

    }

    return vec;
    }