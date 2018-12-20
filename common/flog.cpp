#include <iostream>
#include <fstream>

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <flog.hpp>

FileLog::FileLog(std::string fname, bool clear) :
	_fname(fname)
{
	std::ios_base::openmode flags;

	if (clear)
		flags = std::ofstream::out | std::ofstream::trunc;
	else
		flags = std::ofstream::out | std::ofstream::app;
	_log_stream.open(_fname, flags);
}

void FileLog::log(std::string msg)
{
	struct timeval tv;
	struct tm *ptm;
	char time_string[40];
	char buf[64];
	int milliseconds;

	gettimeofday(&tv, NULL);
	ptm = localtime(&tv.tv_sec);
	strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", ptm);

	milliseconds = tv.tv_usec / 1000;
	sprintf(buf, "%s.%03d", time_string, milliseconds);
    std::string s(buf);

//    if (s.size() && '\n' == s[s.size() - 1] )
//    	s[s.size() - 1] = ' ';
    s += ": " + msg + "\n";
	_log_stream.write(s.c_str(), s.size());
}

FileLog::~FileLog()
{
	_log_stream.close();
}
