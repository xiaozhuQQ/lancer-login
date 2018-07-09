
#ifndef METEO_GRPC_CLIENT_H
#define METEO_GRPC_CLIENT_H

#include <functional>
#include <stdexcept>
#include <boost/utility.hpp>
#include <boost/type_traits.hpp>
#include <memory>
#include <iostream>
#include <cmath>
#include "assert.h"

#include <grpc++/grpc++.h>
#include <thread>

#include "cpp/lancerLogin.grpc.pb.h"
#include "TokenManager.h"

using grpc::Channel;
using grpc::ClientAsyncReader;
using grpc::ClientAsyncReaderWriter;
using grpc::ClientAsyncResponseReader;
using grpc::ClientAsyncWriter;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;

using namespace lancer;
using namespace std;

class AsyncClientCallBase
{
  public:
	enum CallStatus
	{
		PROCESS,
		FINISH,
		DESTROY
	};

	explicit AsyncClientCallBase() : callStatus(PROCESS) {}
	virtual ~AsyncClientCallBase() 
	{
		cout << "client call instance delelte!" << endl;
	}
	ClientContext context;
	Status status;
	CallStatus callStatus;

	virtual void Proceed(bool = true) = 0;
};

class AsyncRegisterCall : public AsyncClientCallBase
{
	std::unique_ptr<ClientAsyncResponseReader<RegisterRsp>> responder;
	RegisterRsp reply_;

  public:
	AsyncRegisterCall(const RegisterReq &request, CompletionQueue &cq_, std::unique_ptr<LancerLoginServer::Stub> &stub_)
		: AsyncClientCallBase()
	{
		responder = stub_->AsyncRegister(&context, request, &cq_);
		responder->Finish(&reply_, &status, (void *)this);
		callStatus = PROCESS;
	}
	virtual void Proceed(bool ok = true) override
	{
		if (callStatus == PROCESS)
		{
			if (status.ok())
			{
				cout << reply_.msg() << endl;
			}

			std::cout << "---Register ending---" << std::endl;
			delete this;
		}
	}
};

class AsyncLoginCall : public AsyncClientCallBase
{
	std::unique_ptr<ClientAsyncResponseReader<LoginRsp>> responder;
	LoginReq req_;
	LoginRsp reply_;

  public:
	AsyncLoginCall(const LoginReq &request, CompletionQueue &cq_, std::unique_ptr<LancerLoginServer::Stub> &stub_) : AsyncClientCallBase()
	{
		std::cout << "[Proceed Login]: new client" << std::endl;
		req_ = request;
		responder = stub_->AsyncLogin(&context, request, &cq_);
		responder->Finish(&reply_, &status, (void *)this);
		callStatus = PROCESS;
	}
	virtual void Proceed(bool ok = true) override
	{
		if (callStatus == PROCESS)
		{
			if (status.ok())
			{
				cout << reply_.msg() << endl;
				if (reply_.ret() == E_SUCCESS)
				{
					TokenManager::getInstance().saveToken(req_.username(), reply_.token());
				}
			}

			cout << "[Proceed Login]: end" << std::endl;
			delete this;
		}
	}
};

class AsyncNotifyCall : public AsyncClientCallBase
{
	std::unique_ptr<ClientAsyncReaderWriter<NotifyReq, NotifyRsp>> responder;
	bool writing_mode_;
	bool write_done;
	NotifyRsp reply_;
	string usrID_;
	string guid_;
	bool isMe_;
	bool isFirstBuild;

  public:
	AsyncNotifyCall(const string guid, const string usrid, CompletionQueue &cq_, std::unique_ptr<LancerLoginServer::Stub> &stub_) : 
	AsyncClientCallBase(), writing_mode_(false), guid_(guid), usrID_(usrid), isFirstBuild(false)
	{
		std::cout << "[Proceed Notify]: new client" << std::endl;
		responder = stub_->AsyncNotify(&context, &cq_, (void *)this);
		callStatus = PROCESS;
		write_done = false;
	}
	virtual void Proceed(bool ok = true) override
	{
		cout <<"----PPP----	OK" << ok <<endl;
		if (callStatus == PROCESS)
		{

			if (writing_mode_)
			{
				if (!write_done)
				{
					//已经接受到通知
					if (isMe_)
					{
						NotifyReq request;
						request.set_flag(E_CLIENT_CLOSE);
						string token = TokenManager::getInstance().acquireToken(usrID_);
						request.set_token(token);
						request.set_username(usrID_);
						responder->Write(request, (void *)this);
						write_done = true;
					}
				}
				else
				{
					cout << "client write end" << endl;
					responder->WritesDone((void *)this);
					isMe_ = false;
					callStatus = FINISH;
					// writing_mode_ = false;
				}
				return;
			}
			else 
			{
				if (!ok)
				{
					std::cout << "changing state to writing" << std::endl;
					// callStatus = FINISH;
					writing_mode_ = true;
					responder->Finish(&status, (void *)this);
					return;
				}

				responder->Read(&reply_, (void *)this);
				cout << "GID|" << reply_.guid() << endl;
				cout << "flag|" << reply_.flag() << endl;
				isMe_ = (reply_.guid() == guid_) && (reply_.flag() == E_OFFLINE_PUSH);

				if (isMe_)
				{
					cout << "local GUID|" + guid_ + "|will close the channel" << endl;
					callStatus = FINISH;
				}
			}
			return;
		}
		else if (callStatus == FINISH)
		{
			std::cout << "[Proceed Notify]: END" << std::endl;
			delete this;
		}
	}
};

class Client
{
  public:
	explicit Client(std::shared_ptr<Channel> channel)
		: stub_(LancerLoginServer::NewStub(channel))
	{
		channel_ = channel;
	}

	void Register(const std::string &user, const std::string &password)
	{
		RegisterReq req;
		req.set_username(user);
		req.set_password(password);
		cout << "\nbegin to register:" << endl;
		cout << "-->user name:" << user << "\n";
		cout << "-->password:" << password << "\n";

		new AsyncRegisterCall(req, cq_, stub_);
	}

	void Login(const std::string &user, const std::string &password, const std::string &guid)
	{
		LoginReq req;
		req.set_username(user);
		req.set_password(password);
		req.set_guid(guid);
		cout << "\nbegin to login:" << endl;
		cout << "-->user name:" << user << "\n";
		cout << "-->password:" << password << "\n";
		cout << "-->guid:" << guid << endl;
		new AsyncLoginCall(req, cq_, stub_);
	}

	void NotifyServer(const std::string &usrID, const std::string &guid)
	{
		new AsyncNotifyCall(guid, usrID, cq_, stub_);
	}

	void AsyncCompleteRpc()
	{
		void *got_tag;
		bool ok = false;
		while (cq_.Next(&got_tag, &ok))
		{
			AsyncClientCallBase *call = static_cast<AsyncClientCallBase *>(got_tag);
			call->Proceed(ok);
		}
		std::cout << "Completion queue is shutting down." << std::endl;
	}

  private:
	std::unique_ptr<LancerLoginServer::Stub> stub_;

	CompletionQueue cq_;

	std::shared_ptr<Channel> channel_;
};

int main(int argc, char *argv[])
{

	Client cc(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
	std::thread thread_ = std::thread(&Client::AsyncCompleteRpc, &cc);

	// cc.Register("lancer33", "12342s56");

	// cc.Login("lancer33","12342s56","AABBCCDDEE");//错误登录
	// cc.NotifyServer("lancer33","AABBCCDDEE");

	// cc.Login("lancer11","12342s56","EEFFGGHHII");//错误登录

	cc.Login("lancer33", "12342s56", "EEFFGGHHII"); //正确登录
	cc.NotifyServer("lancer33", "EEFFGGHHII");
	thread_.join();
}

#endif
