#ifndef __DBMANAGER_H__
#define __DBMANAGER_H__

#include <iostream>
#include <string>
#include <mysql_connection.h>  
#include <mysql_driver.h>  
#include <cppconn/exception.h>  
#include <cppconn/driver.h>  
#include <cppconn/connection.h>  
#include <cppconn/resultset.h>  
#include <cppconn/prepared_statement.h>  
#include <cppconn/statement.h>  
#include <pthread.h>  

using namespace std;
using namespace sql;

struct UserInfo
{
    std::string usrID;
    std::string pw;
    std::string token;
    std::string GUID;
};
enum DevState
{
    ONLINE = 10,//该设备登录
    D_ONLINE,//其他设备登录
    OFFLINE,//下线
    FORBIDDEN//禁止
};
class DBManager 
{
    public:
        static DBManager& getInstance();

        void initDB();

        //注册用户信息
        int registUserInfoToDB(const UserInfo usrInfo);

        //根据用户ID查找信息
        int selectUserInfoByUsrID(const std::string usrID, UserInfo &info);

        //检查用户是否在某一设备是否登录
        int checkUserLoginState(const std::string usrID, const string curDeviceID, std::string &loginedID, DevState& state);

        //插入用户登录信息
        int insertLoginInfoToDb(const std::string usrID, const std::string curDeviceID, DevState &state);


        //更改用户登录状态
        int modifyUserLoginStateInfo(const std::string usrID, const std::string curDeviceID, const DevState state, const string token);

        virtual ~DBManager();

    protected:
        DBManager();

    private:
        //sql
        sql::Connection *con_;
        sql::mysql::MySQL_Driver *driver_;
        Statement *state_;
        ResultSet *result;

        mutex db_lock_;
};

#endif