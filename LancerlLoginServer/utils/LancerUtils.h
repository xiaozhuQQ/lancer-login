#ifndef _LANCEAR_UTILS_H_
#define _LANCEAR_UTILS_H_


#include <iostream>
#include <string>
#include <sstream>
#include <regex>
#include <sys/time.h>

#include "MD5.h"
using namespace std;

static bool checkAccountFormat(const std::string usrName)
{
    bool lengthOK = usrName.size() > 0 && usrName.size() < 18;
    //格式校验

    return lengthOK;
}

static bool checkPasswodFormat(const std::string password)
{
    regex reg("^(?![0-9]+$)(?![a-zA-Z]+$)[0-9A-Za-z]{8,16}$");
    smatch m;
    return  regex_match(password, m, reg);
}

static string generateToken(const std::string id)
{
    string token;

    struct timeval tv;    
    gettimeofday(&tv,NULL);
    int64_t now =  tv.tv_sec * 1000 + tv.tv_usec / 1000;
    stringstream os;
    os << now;
    string timeStr = os.str();

    Md5Encode md5;
	token = md5.Encode(id+ timeStr);
    cout<< "new token::" << token<< endl;
    return token;
}

#endif