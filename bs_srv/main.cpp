/* Server code in C */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>

#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include <memory>
#include <thread>

#include <string.h>	// memset
#include <unistd.h>	// close

#include <fcntl.h>
#include <errno.h>

#undef ENABLE_DEBUG_OUTPUT

#define ENABLE_INFO_OUTPUT
#include <mydef.h>

#include <main.hpp>
#include <flog.hpp>
#include <server.hpp>
#include <common.hpp>

// -------------------------

typedef struct
{
  int full_buf_read;
  int full_packet_read;
  int nothing_read;
  int smth_else;
} rcv_buf_stat;

using namespace std;

int main(int argc, char *argv[])
{
  pid_t process_id = getpid();
  string filename = string("server_") + to_string((int) process_id);

  command_arguments_t args = {
		  .w_flag = 0, .rcv_size = RCVBUF_SIZE, .snd_size = SNDBUF_SIZE, .l_flag = 0,
		  .packet_size = PACKET_SIZE, .n_flag = 0, .packets_nmb = 0,
		  .b_flag = 0, .server_ip = string("127.0.0.1"),
		  .c_flag = 0, .ip = nullptr, .p_flag = 0, .port_nmb = 0,
		  .threads = 0, .fname = filename,
		  .client_ip = std::string(""),
		  .i_flag = 0, .inaddr = std::string("")
  };

  int opt;

  signal(SIGPIPE, SIG_IGN);

  // Parse command line
  while ((opt = getopt(argc, argv, "hl:w:p:b:i:")) != -1)
  {
    switch (opt)
    {
      case 'l':
        args.l_flag = 1;
        args.packet_size = atoi(optarg);
        break;
      case 'w':
        args.w_flag = 1;
        args.rcv_size = atoi(optarg);
        args.snd_size = atoi(optarg);
        break;

      case 'p':
        args.p_flag = 1;
        args.port_nmb = atoi(optarg);
        break;
      case 'b':
    	args.b_flag = 1;
    	args.server_ip = std::string(optarg);
    	break;
      case 'i':
      	args.i_flag = 1;
      	args.inaddr = std::string(optarg);
    	break;
      case 'h':
        F_INFO("Server application based on blocking sockets\nUsage: bs_srv [options]\n");
        F_INFO(" -l <size>        packet size (1572864 bytes by default)\n");
        F_INFO(" -w <size>        socket buffer size\n");

        F_INFO(" -p <port>        port number\n");
        exit(EXIT_SUCCESS);
        break;
      default:
        break;
    }
  }

  {
	  FileLog fl(string("server_") + args.server_ip, true);
	  fl.log(string("Start"));
  }


  do  //Once
  {
    std::shared_ptr<server_data_t> server_data = std::make_shared<server_data_t>();
    std::shared_ptr<command_arguments_t> p_args = std::make_shared<command_arguments_t>(args);
	F_DBG("Checkpoint 1\n");

	std::chrono::steady_clock::time_point timepoint_start = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point timepoint_log = std::chrono::steady_clock::now();
	std::string fault_reason("No error");
	while (EXIT_FAILURE == server_activity(p_args, timepoint_start, fault_reason))
	{
		  int milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timepoint_start).count();
		  if (milliseconds > SERVER_TIMEOUT * 1000)
		  {
			  {
				  FileLog fl(string("server_") + args.server_ip, false);
				  fl.log(string("Server isn't receiving new packets for more than last ") +
						  to_string(SERVER_TIMEOUT) + " seconds. The test failed.");
			  }
			  break;
		  }
		  else
		  {
			  if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - timepoint_log).count() > 1)
			  {
				  timepoint_log = std::chrono::steady_clock::now();
				  {
					  FileLog fl(string("server_") + args.server_ip, false);
					  fl.log(string("Server thread failure, reason - ") + fault_reason +
							  ", " + to_string(milliseconds / 1000) +
							  " seconds without packets, reconnecting...");
				  }
			  }
		  }
	}

	F_MSG("Thread has been joined, server = %s\n", args.server_ip.c_str());
    F_DBG("Closing socket\n");
  } while (0);

  return 0;
}

