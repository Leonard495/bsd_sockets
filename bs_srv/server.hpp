#ifndef SERVER_H_
#define SERVER_H_

#include <memory>

enum
{
	SERVER_NO_COMMAND,
	SERVER_END_OF_PACKET,
	SERVER_END_OF_SESSION
};

enum
{
	SNDBUF_SIZE = 131072,
	RCVBUF_SIZE = 131072,
	PACKET_SIZE = 1572864
};

typedef struct
{
  int               packet_size;
  int               thread_nmb;
  int				cmd;
} server_data_t;

int server_activity(std::shared_ptr<command_arguments_t> p_args,
		std::chrono::steady_clock::time_point &timepoint_start,
		std::string& fault_reason);

#endif
