/*
 * mydef.h
 *
 *  Created on: Nov 25, 2015
 *      Author: leonard
 */

#ifndef MYDEF_H_
#define MYDEF_H_

#include <string>


#ifdef ENABLE_DEBUG_OUTPUT
# define  F_DBG(args...)															\
{																						\
	char buf[1024];																		\
	std::string msg = std::string(__FILE__) + ", " + std::to_string(__LINE__) + ": ";	\
	sprintf(buf, ##args);																\
	msg += buf;																			\
	msg += "\n";																		\
	printf("%s", msg.c_str());																\
}
#else

# define  F_DBG(args...)

#endif

#ifdef ENABLE_INFO_OUTPUT
# define  F_INFO(s)                                   printf(s)

# define  F_MSG(args...)																\
{																						\
	char buf[1024];																		\
	std::string msg = std::string(__FILE__) + ", " + std::to_string(__LINE__) + ": ";	\
	sprintf(buf, ##args);																\
	msg += buf;																			\
	msg += "\n";																		\
	printf("%s", msg.c_str());															\
}

# define  F_MSG_SHORT(args...)															\
{																						\
	char buf[1024];																		\
	sprintf(buf, ##args);																\
	printf("%s", buf);																	\
}

#else
# define  F_INFO(s)
# define  F_MSG(fmt, args...)
# define  F_MSG_SHORT(args...)
#endif

enum {ACK_SIZE = 1};

enum
{
  SOCK_SHUTDOWN_COMMAND = 1, SOCK_CLOSE_COMMAND, SOCK_START_TEST, SERVER_PACKET_TIMEOUT,
  BASIC_PORT = 1100, CLIENTS_MAX_NMB = 1,
};

#endif /* MYDEF_H_ */
