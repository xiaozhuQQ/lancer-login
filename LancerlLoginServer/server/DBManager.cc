#include "DBManager.h"

DBManager &DBManager::getInstance()
{
    static DBManager instance;
    return instance;
}

DBManager::DBManager()
{
}

DBManager::~DBManager()
{
    delete con_;
    delete driver_;
    delete state_;
}

void DBManager::initDB()
{
    try
    {
        driver_ = sql::mysql::get_mysql_driver_instance();
        con_ = driver_->connect("tcp://localhost:3306/db_lancer_login", "root", "888888");
        state_ = con_->createStatement();
        cout << "db connect success" << endl;
    }
    catch (...)
    {
        cout << "db connect faild" << endl;
    }
}

//注册用户信息
int DBManager::registUserInfoToDB(const UserInfo usrInfo)
{
    try
    {
        cout << "id:" << usrInfo.usrID << endl;
        cout << "pw:" << usrInfo.pw << endl;
        sql::PreparedStatement *ps;
        string sql = "insert into t_user(usr_id,usr_pw) VALUES (?,?)";
        ps = con_->prepareStatement(sql);
        ps->setString(1, usrInfo.usrID);
        ps->setString(2, usrInfo.pw);

        db_lock_.lock();
        ps->executeUpdate();
        db_lock_.unlock();    }
    catch (...)
    {
        cout << "[insert] user info to db error" << endl;
        return -1;
    }

    return 0;
}

//根据用户ID查找信息
int DBManager::selectUserInfoByUsrID(const std::string usrID, UserInfo &info)
{
    try
    {
        ResultSet *res;
        string sql = "select * from t_user where usr_id = '" + usrID + "'";
        res = state_->executeQuery(sql);
        while (res->next())
        {
            info.usrID = res->getString("usr_id");
            info.pw = res->getString("usr_pw");
            cout << "id:" << info.usrID << endl;
            cout << "pw:" << info.pw << endl;
        }
    }
    catch (...)
    {
        cout << "[select] user info form t_user error" << endl;
        return -1;
    }

    return 0;
}

int DBManager::insertLoginInfoToDb(const std::string usrID, const std::string curDeviceID, DevState &state)
{
    try
    {
        sql::PreparedStatement *ps;
        string sql = "insert into t_device(usr_id, guid, state) VALUES (?,?,?)";
        ps = con_->prepareStatement(sql);
        ps->setString(1, usrID);
        ps->setString(2, curDeviceID);
        ps->setInt(3, state);

        db_lock_.lock();
        ps->executeUpdate();
        db_lock_.unlock();
    }
    catch (...)
    {
        cout << "[insert] device info to db error" << endl;
        return -1;
    }
    return 0;
}

//检查用户登录状态是否在某一设备是否登录
int DBManager::checkUserLoginState(const std::string usrID, const std::string curDeviceID, std::string& loginedDevID,DevState &state)
{
    try
    {
        ResultSet *res;
        map <string, DevState> devStateList;
        //找出所有该用户的设备信息
        string sql = "select * from t_device where usr_id = '" + usrID + "'";
        res = state_->executeQuery(sql);
        while (res->next())
        {
            string g = res->getString("guid");
            DevState s = (DevState)res->getInt("state");
            devStateList.insert(make_pair(g ,s));
            cout<< "guid:" << g <<endl;
            cout<< "state:" << s <<endl;
        }

        if (devStateList.find(curDeviceID) == devStateList.end())
        {
            //没有找到当前设备的状态信息
            cout<<"没有找到当前设备的状态信息"<<endl;
            state = OFFLINE;
            //插入一条记录
            DBManager::getInstance().insertLoginInfoToDb(usrID, curDeviceID, state);
        }
        
        
        if(devStateList[curDeviceID] == ONLINE)
        {
            //当前设备已经登录
            cout<<"当前设备已经登录"<<endl;
            state = ONLINE;
        }
        else
        {
            //检查是否有其他设备登录
            state = OFFLINE;
            map<string,DevState>::iterator it;
            for(it = devStateList.begin() ;it != devStateList.end(); it++)
            {
                if(it->second == ONLINE)
                {
                    state = D_ONLINE;
                    loginedDevID = it->first;
                    cout<<"其他设备已经登录 该设备为：" << loginedDevID <<endl;
                    break;
                }
            }     
        }
    }
    catch (...)
    {
        cout << "[select] check user logined state error" << endl;
        return -1;
    }

    return 0;
}

//更改用户登录状态
int DBManager::modifyUserLoginStateInfo(const std::string usrID, const std::string curDeviceID, const DevState state, const string token)
{
    try
    {
        sql::PreparedStatement *ps;
        string sql = " update t_device set state = ?, token = ? where usr_id = ? and guid = ?";
        ps = con_->prepareStatement(sql);
        
        ps->setInt(1, state);
        ps->setString(2,token);
        ps->setString(3,usrID);
        ps->setString(4,curDeviceID);
        
        db_lock_.lock();
        ps->executeUpdate();
        db_lock_.unlock();
    }
    catch (...)
    {
        cout << "[update] device info to db error" << endl;
        return -1;
    }
    return 0;
}