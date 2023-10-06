#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

// User表的数据操作类
class UserModel
{
public:
  bool insert(User& user);

  User query(int id);

  //更新用户的状态
  bool updateState(User user);

  //重置用户的状态信息
  void resetState();
};


#endif