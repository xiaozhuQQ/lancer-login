#include <iostream>
#include <string>
#include <map>

using namespace std;
class TokenManager
{
	public:
		static TokenManager& getInstance()
		{
			static TokenManager instance;
    		return instance;
		}
		~TokenManager(){}

		void saveToken(const string key, const string token)
		{
			if(tokenHolder_.find(key) != tokenHolder_.end())
			{
				mt.lock();
				tokenHolder_[key]= token;
				mt.unlock();
			}
			else
			{
				mt.lock();
				tokenHolder_.insert(make_pair(key,token));
				mt.unlock();
			}
			
		}
		string acquireToken(const string key)
		{
			if(tokenHolder_.find(key) != tokenHolder_.end())
			{
				return tokenHolder_[key];
			}
			return "";
		}

		void clearToken(const string key)
		{
			map<string, string>::iterator it = tokenHolder_.find(key);
			if(it != tokenHolder_.end())
			{	
				mt.lock();
				tokenHolder_.erase(it);
				mt.unlock();
			}
		}
	private:
		TokenManager (){};

		std::mutex mt;
		
		map<string,string> tokenHolder_;
};
