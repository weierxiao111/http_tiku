/*************************************************************************
	> File Name: question_cgi.cpp
	> Author: weierxiao
	> Mail: 1091868910@qq.com 
	> Created Time: Fri 28 Jul 2017 09:35:18 PM CST
 ************************************************************************/

#include<iostream>
#include "sql.h"
using namespace std;

int main()
{
	string result;
	sql res("wx","127.0.0.1", "666","questionbank" ,3306);
	res.connect();
	res.select(result);
	res.Get(result);
	res.clear_res();
	return 0;

}
