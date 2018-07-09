
#ifndef METEO_GRPC_SERVER_H
#define METEO_GRPC_SERVER_H

#include <functional>
#include <stdexcept>
#include <boost/utility.hpp>
#include <boost/type_traits.hpp>
#include <memory>
#include <iostream>
#include <cmath>
#include "assert.h"

#include <grpc++/grpc++.h>

#include "cpp/lancerLogin.grpc.pb.h"
#include "DBManager.h"
#include "utils/MD5.h"
#include "utils/LancerUtils.h"

using namespace lancer;
using namespace grpc;

class CommonCallData
{
  public:
	LancerLoginServer::AsyncService *service_;

	ServerCompletionQueue *cq_;

	ServerContext ctx_;

	enum CallStatus
	{
		CREATE,
		PROCESS,
		FINISH
	};

	CallStatus status_;

	std::string prefix;

  public:
	explicit CommonCallData(LancerLoginServer::AsyncService *service, ServerCompletionQueue *cq) : service_(service), cq_(cq), status_(CREATE), prefix("Hello ")
	{
	}

	virtual ~CommonCallData()
	{
		std::cout << "CommonCallData destructor" << std::endl;
	}

	virtual void Proceed(bool = true) = 0;
};

class RegisterCallData : public CommonCallData
{
	ServerAsyncResponseWriter<RegisterRsp> responder_;
	RegisterReq request_;
	RegisterRsp reply_;

  public:
	RegisterCallData(LancerLoginServer::AsyncService *service, ServerCompletionQueue *cq) : CommonCallData(service, cq), responder_(&ctx_) { Proceed(); }

	virtual void Proceed(bool = true) override
	{
		if (status_ == CREATE)
		{
			status_ = PROCESS;
			service_->RequestRegister(&ctx_, &request_, &responder_, cq_, cq_, this);
		}
		else if (status_ == PROCESS)
		{
			new RegisterCallData(service_, cq_);

			std::cout << "---" << request_.username() << " begin to register--- " << std::endl;
			UserInfo userInfo;
			DBManager::getInstance().selectUserInfoByUsrID(request_.username(), userInfo);
			if (userInfo.usrID != request_.username())
			{
				if (!checkAccountFormat(request_.username()))
				{
					reply_.set_ret(E_ILLEGAL_ACCT);
					reply_.set_msg("账号格式错误");
				}
				else if (!checkPasswodFormat(request_.password()))
				{
					reply_.set_ret(E_ILLEGAL_PW);
					reply_.set_msg("密码格式错误");
				}
				else
				{
					//加密密码
					Md5Encode md5;
					string newPw = md5.Encode(request_.password());

					//插入数据库
					userInfo.usrID = request_.username();
					userInfo.pw = newPw;
					if (DBManager::getInstance().registUserInfoToDB(userInfo) >= 0)
					{
						reply_.set_ret(E_SUCCESS);
						reply_.set_msg("账号注册成功");
					}
					else
					{
						reply_.set_ret(E_DB_ERROR);
						reply_.set_msg("数据库异常");
					}
				}
			}
			else
			{
				//已经注册
				reply_.set_ret(E_DUPLICATE_ACCT);
				reply_.set_msg("账号已经注册");
			}

			status_ = FINISH;
			responder_.Finish(reply_, Status::OK, this);
		}
		else
		{
			if (status_ == FINISH)
			{
				std::cout << "[Proceed11]: Good Bye" << std::endl;
			}

			delete this;
		}
	}
};

class NotifyCallData : public CommonCallData
{
	ServerAsyncReaderWriter<NotifyRsp, NotifyReq> responder_;
	bool writing_mode_;
	bool write_done_;
	bool new_responder_created;
	NotifyReq request_;
	NotifyRsp reply_;

	string guid_;

  public:
	NotifyCallData(LancerLoginServer::AsyncService *service, ServerCompletionQueue *cq, const string &guid) : CommonCallData(service, cq), responder_(&ctx_), writing_mode_(false), new_responder_created(false), guid_(guid)
	{
		Proceed();
	}

	virtual void Proceed(bool ok = true) override
	{
		if (status_ == CREATE)
		{
			std::cout << "[Notify]: New responder for notify pipe" << std::endl;
			status_ = PROCESS;
			service_->RequestNotify(&ctx_, &responder_, cq_, cq_, this);
		}
		else if (status_ == PROCESS)
		{
			
			std::cout << "[Notify]: ok|" << ok << "|wm|" << writing_mode_ << endl;
			if (!writing_mode_) //reading mode
			{
				if (!ok)
				{
					cout << "[Notify]:set status to finish" << endl;
					ok = true;
					status_ = FINISH;
				}
				else
				{
					responder_.Read(&request_, (void *)this);
					cout << "[Notify]: handle ack data" << request_.username() << endl;
					// if (request_.flag() == E_CLIENT_CLOSE)
					// {
					// 	//客户端断开链接，更改数据状态
					// 	DBManager::getInstance().modifyUserLoginStateInfo(request_.username(), guid_, OFFLINE, "");
					// 	std::cout << "[Notify]:modify dev " << guid_ << " state in db" << std::endl;
					// }
				}
			}
			if (writing_mode_) //writing mode
			{
				if (!ok || write_done_)
				{
					std::cout << "[Notify]: write end" << endl;
					writing_mode_ = false;
					status_ = FINISH;
					responder_.Finish(Status(), (void *)this);
				}
				else
				{
					reply_.set_flag(E_OFFLINE_PUSH);
					reply_.set_guid(guid_);
					responder_.Write(reply_, (void *)this);
					std::cout << "[Notify]:" << guid_ << " ,please close your client" << endl;
					write_done_ = true;
				}
			}
		}
		else
		{
			std::cout << "[Notify]: Good Bye" << std::endl;
			delete this;
		}
	}
};

class LoginCallData : public CommonCallData
{
	ServerAsyncResponseWriter<LoginRsp> responder_;
	LoginReq request_;
	LoginRsp reply_;

  public:
	LoginCallData(LancerLoginServer::AsyncService *service, ServerCompletionQueue *cq) : CommonCallData(service, cq), responder_(&ctx_) { Proceed(); }

	virtual void Proceed(bool = true) override
	{
		if (status_ == CREATE)
		{
			std::cout << "[LOGIN]: New responder Create" << std::endl;
			status_ = PROCESS;
			service_->RequestLogin(&ctx_, &request_, &responder_, cq_, cq_, this);
		}
		else if (status_ == PROCESS)
		{
			new LoginCallData(service_, cq_);
			std::cout << "-------" << request_.username() << " begin to login---- " << std::endl;

			UserInfo userInfo;
			DBManager::getInstance().selectUserInfoByUsrID(request_.username(), userInfo);
			Md5Encode md5;
			//先验证账号密码
			if (userInfo.usrID == request_.username() && userInfo.pw == md5.Encode(request_.password()))
			{
				//获取请求的GUID
				string curLoginingDev = request_.guid();
				string loginedDev;
				if (!curLoginingDev.empty())
				{
					DevState state;
					string newToken;

					//检查登录态
					DBManager::getInstance().checkUserLoginState(userInfo.usrID, curLoginingDev, loginedDev, state);
					switch (state)
					{
					case ONLINE:
						//当前设备已经登录
						reply_.set_ret(E_DEV_DUPLICATE_LOGIN);
						reply_.set_msg("帐号已经登录");
						break;
					case D_ONLINE:
						//其他设备已经登录
						//去通知其他端
						new NotifyCallData(service_, cq_, loginedDev);
						DBManager::getInstance().modifyUserLoginStateInfo(request_.username(), loginedDev, OFFLINE, "");
						goto LOGIN;
						break;

					case FORBIDDEN:
						//设备被禁止
						reply_.set_ret(E_DEV_FORBIDDEN);
						reply_.set_msg("此账号禁止登录");
						break;
					case OFFLINE:
						//设备未登录

						LOGIN:
						//生成token
						newToken = generateToken(userInfo.usrID);

						//生成新的token
						if (DBManager::getInstance().modifyUserLoginStateInfo(userInfo.usrID, curLoginingDev, ONLINE, newToken) >= 0)
						{
							reply_.set_ret(E_SUCCESS);
							reply_.set_token(newToken);
							reply_.set_msg("登录成功");
						}
						else
						{
							reply_.set_ret(E_DB_ERROR);
							reply_.set_msg("数据库异常");
						}

						break;
					default:
						break;
					}
				}
				else
				{
					//设备信息为空
					reply_.set_ret(E_ACCT_ERROR);
					reply_.set_msg("设备信息异常");
				}
			}
			else
			{
				//账号密码错误
				reply_.set_ret(E_ACCT_ERROR);
				reply_.set_msg("账号或者密码错误");
			}
			status_ = FINISH;
			responder_.Finish(reply_, Status::OK, this);
		}
		else
		{
			if (status_ == FINISH)
			{
				std::cout << "[LOGIN]: Good Bye" << std::endl;
			}

			delete this;
		}
	}
};

class ServerImpl
{
  public:
	~ServerImpl()
	{
		server_->Shutdown();
		cq_->Shutdown();
	}

	void Run()
	{
		//初始化数据库
		DBManager::getInstance().initDB();

		std::string server_address("0.0.0.0:50051");

		ServerBuilder builder;
		builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

		builder.RegisterService(&service_);

		cq_ = builder.AddCompletionQueue();

		server_ = builder.BuildAndStart();
		std::cout << "Server listening on " << server_address << std::endl;

		new RegisterCallData(&service_, cq_.get());
		new LoginCallData(&service_, cq_.get());
		// new NotifyCallData(&service_, cq_.get());

		void *tag;
		bool ok;
		while (true)
		{
			if (cq_->Next(&tag, &ok))
			{
				CommonCallData *calldata = static_cast<CommonCallData *>(tag);
				calldata->Proceed(ok);
			}
		}
	}

  private:
	std::unique_ptr<ServerCompletionQueue> cq_;
	LancerLoginServer::AsyncService service_;
	std::unique_ptr<Server> server_;
};

int main(int argc, char *argv[])
{
	ServerImpl server;
	server.Run();
}

#endif
