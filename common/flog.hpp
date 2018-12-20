#ifndef __FLOG_HPP__
#define __FLOG_HPP__

#include <iostream>
#include <fstream>

class FileLog
{
public:
	FileLog(std::string _fname, bool clear);
	~FileLog();
public:
	void log(std::string msg);
private:
	std::string		_fname;
	std::ofstream	_log_stream;
};

#endif
