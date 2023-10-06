#include "db.h"
#include <muduo/base/logging.h>

MySQL::MySQL()
  {
    _conn = mysql_init(nullptr);
  }
  MySQL::~MySQL()
  {
    if(_conn != nullptr)
    {
        mysql_close(_conn);
    }
  }

  bool MySQL::connect() // 连接数据库
  {
    MySQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(), password.c_str(), dbname.c_str(),
                                   3306, nullptr, 0);
    if(p != nullptr) {

        // C/C++默认编码 ASCII
        mysql_query(_conn, "set names gbk");

        LOG_INFO << "connect mysql success!";
    }
    else {
        LOG_INFO << "connect mysql failed!";
    }

    return p;
  }

  bool MySQL::update(string sql)
  {
    if(mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
          << sql << "更新失败!";
        return false;
    }
    return true;
  }

  MYSQL_RES* MySQL::query(string sql)
  {
    if(mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
          << sql << "查询失败!";
        return false;
    }
    return mysql_use_result(_conn);
  }

  MySQL* MySQL::getConnection(){
    return _conn;
  }