/* Client code in C */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>

#include <fcntl.h>		// For O_ constants
#include <sys/stat.h>	// For mode constants

#include <signal.h>

#include <string.h>	// memset
#include <unistd.h>	// close

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>

#include <initializer_list>

#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <pthread.h>

#undef  ENABLE_DEBUG_OUTPUT
#define ENABLE_INFO_OUTPUT

#include <mydef.h>

#include <main.hpp>
#include <clients.h>
#include <common.hpp>

#include <flog.hpp>

// Default parameters values
enum
{
	SNDBUF_SIZE = 425984,
	RCVBUF_SIZE = 425984,
	PACKET_SIZE = 1572864
};
// -------------------------

#define TEST_TIME_INTERVAL_SEC  5

typedef struct
{
  int written_full_buf;
  int written_full_packet;
  int nothing_written;
  int smth_else;
} snd_buf_stat;

using namespace std;

std::once_flag once;

SharedSemaphore::SharedSemaphore(const char* name) :
		sema_name(name)
{
	sema = sem_open(sema_name.c_str(), O_CREAT | O_EXCL, O_RDWR, 1);
	if (SEM_FAILED == sema)
	{	// Semaphore exists already
		if (EEXIST == errno)
		{
			sema = sem_open(sema_name.c_str(), 0);
		}
		else
		{
			std::string info = std::string("Semaphore error ") + to_string(errno);
			throw std::runtime_error(info.c_str());
		}
	}
}

SharedSemaphore::~SharedSemaphore()
{
	sem_close(sema);
	sem_unlink(sema_name.c_str());
}

void SharedSemaphore::post(void)
{
	sem_post(sema);
}

void SharedSemaphore::wait(void)
{
	sem_wait(sema);
}

int main(int argc, char *argv[])
{
	string ip_str("127.0.0.1");
	int packet_size = PACKET_SIZE;
	int port_nmb;
	int disconnected = 0;

	signal(SIGPIPE, SIG_IGN);

	std::shared_ptr<SharedSemaphore> ss = std::make_shared < SharedSemaphore
			> (SHARED_SEMA_NAME);

	{
		FileLog fl(string("speed_problems"), true);
		fl.log("Start");
	}

	command_arguments_t args =
	{
		.w_flag = 0, .rcv_size = 0, .snd_size = 0, .l_flag = 0,
		.packet_size = 0, .n_flag = 0, .packets_nmb = 0, .b_flag = 0,
		.c_flag = 0, .ip = nullptr, .p_flag = 0, .port_nmb = 0,
		.client_ip = std::string(""), .i_flag = 0, .iterations = 0,
		.t_flag = 1, .time = TEST_TIME_INTERVAL_SEC
	};
	int opt;
	int sbuf_len = SNDBUF_SIZE, rbuf_len = RCVBUF_SIZE;

	enum {
		START,
		CLIENT_DATA_ALLOCATED,
		SOCK_START,
		SOCK_CREATED,
		SOCK_BIND,
		SOCK_CONNECT,
		SOCK_OP_ACCOMPLISHED
	};

	int step = START;

	mallopt(M_MMAP_MAX, 0); // Disable mmap()

	// Parse command line
	while ((opt = getopt(argc, argv, "hb:n:l:w:c:p:i:t:")) != -1) {
		switch (opt) {
		case 'l':
			args.l_flag = 1;
			args.packet_size = atoi(optarg);
			break;
		case 'w':
			args.w_flag = 1;
			args.rcv_size = atoi(optarg);
			args.snd_size = atoi(optarg);
			break;
		case 'n':
			args.n_flag = 1;
			args.packets_nmb = atoi(optarg);
			break;
		case 'b':
			args.b_flag = 1;
			args.client_ip.assign(optarg);
			break;
		case 'c':
			args.c_flag = 1;
			args.ip = optarg;
			break;
		case 'p':
			args.p_flag = 1;
			args.port_nmb = atoi(optarg);
			break;
		case 't':
			args.t_flag = 1;
			args.time = atoi(optarg);
			break;
		case 'i':
			args.i_flag = 1;
			args.iterations = atoi(optarg);
			break;
		case 'h':
			F_INFO(
					"Client application based on blocking sockets\nUsage: bs_cli [options]\n");
			F_INFO(
					" -l <size>        packet size (1572864 bytes by default)\n");
			F_INFO(" -w <size>        socket buffer size\n");
			F_INFO(" -c <host>        ip address to connect\n");
			F_INFO(" -P #             number of parallel thread to run\n");
			F_INFO(" -b <ip-address>  ip address the socket to be binded to\n");
			F_INFO(" -i <iterations>  iterations\n");
			exit (EXIT_SUCCESS);
			break;
		default:
			break;
		}
	}

	if (args.l_flag)
		packet_size = args.packet_size;
	if (args.c_flag)
		ip_str = string(args.ip);
	if (args.w_flag) {
		sbuf_len = args.snd_size;
		rbuf_len = args.rcv_size;
	}
	if (!args.b_flag) {
		args.client_ip = string("127.0.0.1");
	}
	if (!args.i_flag) {
		args.iterations = 1;
	}

	if (!args.p_flag)
		port_nmb = BASIC_PORT;
	else
		port_nmb = BASIC_PORT + args.port_nmb;

	printf("Client IP:port = %s:%d\n", args.client_ip.c_str(), port_nmb);

	F_DBG("\n--------------------- Incoming parameters ------------------\n");
	F_DBG("Packet size = %d\n", packet_size);
	F_DBG("IP address to connect = %s\n", ip_str.c_str());
	F_DBG("Threads number = %d\n", threads);
	F_DBG("Client IP = %s\n", args.client_ip.c_str());
	F_DBG("------------------------------------------------------------\n");

	step = START;

	std::chrono::milliseconds ms_to_sleep(3000);
	std::this_thread::sleep_for(ms_to_sleep);

	{
		FileLog fl(string("client_") + args.client_ip, true);
		fl.log(string("Test start"));
	}
	shared_ptr<client_data_t> client_data;
	vector < thread > t_handlers;

	try
	{
		{
			FileLog fl(string("client_status_") + args.client_ip, true);
			fl.log("Start");
		}

		for (int cnt = 0; cnt < args.iterations; cnt++)
		{
			do  // Once
			{
				// Allocate data for

				std::shared_ptr<client_data_t> p = std::make_shared<
						client_data_t>();
				client_data = std::make_shared<client_data_t>();

				step = CLIENT_DATA_ALLOCATED;

				std::call_once(once, [&]()
					{
						F_MSG("Client connecting from %s to %s, port %d\n",
								args.client_ip.c_str(), ip_str.c_str(), port_nmb);
					}
				);

				step = SOCK_CREATED;

				// Start client threads
				client_data->thread_nmb = 0;
				client_data->seconds = args.time;
				client_data->packet_size = packet_size;
				client_data->ip_str = ip_str;
				client_data->port = port_nmb;
				client_data->args = &args;
				client_data->rbuf_len = rbuf_len;
				client_data->sbuf_len = sbuf_len;
				client_data->client_ip = args.client_ip;
				client_data->ss = ss;

				std::chrono::steady_clock::time_point timepoint_log =
						std::chrono::steady_clock::now();
				std::chrono::steady_clock::time_point timepoint_packet_transmission_start =
						std::chrono::steady_clock::now();
				do	// Endless
				{
					client_activity(client_data, (cnt == args.iterations - 1),
							cnt, args.iterations, disconnected);

					if (EXIT_SUCCESS == client_data->result) {
						break;
					} else {
						disconnected = 1;
						if (std::chrono::duration_cast < std::chrono::seconds
								> (std::chrono::steady_clock::now()
										- timepoint_log).count()
								> CLIENT_UNABLE_TO_BIND_MSG_PERIOD) {
							timepoint_log = std::chrono::steady_clock::now();
							show_status(client_data->client_ip,
									client_data->ip_str, STATUS_DISCONNECTED,
									cnt, args.iterations);
						}
					}

					if (std::chrono::duration_cast < std::chrono::seconds
							> (std::chrono::steady_clock::now()
									- timepoint_packet_transmission_start).count()
							> CLIENT_TIMEOUT) {
						timepoint_log = std::chrono::steady_clock::now();
						show_status(client_data->client_ip, client_data->ip_str,
								STATUS_TIMEOUT, cnt, args.iterations);
						throw std::runtime_error("Server is not responding");
					}

				} while (1);

				step = SOCK_OP_ACCOMPLISHED;
			} while (0);

			t_handlers.clear();
		}
	} catch (std::exception& e) {
		{
			FileLog fl(string("client_") + args.client_ip, false);
			fl.log(string("Abnormal end of program: ") + e.what());
		}
		printf("%s, %d: Unsuspected exception was detected: %s\n", __FILE__,
				__LINE__, e.what());
		F_MSG("Abnormal situation: %s", e.what());
	}
	// Free resources properly
	switch (step) {
	case SOCK_OP_ACCOMPLISHED:
	case SOCK_CONNECT:
	case SOCK_BIND:
	case SOCK_CREATED: {
		close(client_data->socket_fd);
	}
	case CLIENT_DATA_ALLOCATED:
	case START:
	default:
		break;
	}

	{
		FileLog fl(string("client_") + args.client_ip, false);
		fl.log(string("End of test"));
	}

	return EXIT_SUCCESS;
}
