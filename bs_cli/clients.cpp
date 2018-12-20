/* Client code in C */
//#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <string.h>	// memset
#include <unistd.h>	// close

#include <errno.h>

#include <time.h>
#include <ctime>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <vector>
#include <chrono>
#include <chrono>
#include <thread>

#include <pthread.h>
#include <malloc.h>
#include <string.h>

#undef ENABLE_INFO_OUTPUT
#undef ENABLE_DEBUG_OUTPUT
#include <mydef.h>

#include <main.hpp>

#include <clients.h>

#include <common.hpp>

#include <flog.hpp>

using namespace std;

// The functions checks if required amount of seconds passed since start_time
int required_time_passed(int seconds, std::chrono::steady_clock::time_point& tp_start)
{
  int res = 0;
  int dt = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - tp_start).count();

  if (dt > seconds)
    res = 1;

  return res;
}

// The function sends len bytes to socket
// Returns:
// 1 on success
// 0 on failure
static int send_packet(int socket_fd, char* buf, int len, string& client_ip, int packet_id)
{
	int res = EXIT_FAILURE;
	int packet_bytes_sent = 0;
	int bytes_processed = 0;

	if (!buf)
		return res;

	{
		FileLog fl(string("client_dbg_") + client_ip, false);
		fl.log(string("Packet#") + to_string(packet_id) + ", socket_fd#" + to_string(socket_fd) +
				", length = " + to_string(len));
	}

	while (packet_bytes_sent < len)
	{
    	int error = 1;
		F_DBG("Attempt to send");
		bytes_processed = send(socket_fd, &buf[packet_bytes_sent],
				len - packet_bytes_sent, MSG_NOSIGNAL);

		if (-EPIPE == bytes_processed)	// The socket is broken (Broken pipe)
		{
			FileLog fl(string("client_dbg_") + client_ip, false);
			fl.log(string("Write attempt to the broken socket has been detected"));
			break;
		}
		else if (bytes_processed < 0)
		{
			FileLog fl(string("client_dbg_") + client_ip, false);
			fl.log(string("Unknown error: ") + to_string(bytes_processed));
			break;
		}
        do
        {
        	struct timeval timeout;
        	int sel_res;
    		fd_set write_fd_set;

        	FD_ZERO(&write_fd_set);
            FD_SET(socket_fd, &write_fd_set);

        	timeout.tv_sec = CLIENT_SEND_PACKET_PERIOD;
        	timeout.tv_usec = 0;
			sel_res = select(FD_SETSIZE, NULL, &write_fd_set, NULL, &timeout);
			if (sel_res < 0)
			{
				F_MSG("Write select error, send operation timeout");
				{
					FileLog fl(string("client_") + client_ip, false);
					fl.log(string("Write select error, send operation timeout"));
				}
				break;
			}
			else if (0 == sel_res)
			{	// Timeout occured
				{
					FileLog fl(string("client_") + client_ip, false);
					fl.log(string("Timeout occured while sending the packet #") + to_string(packet_id) +
							", " + to_string(packet_bytes_sent) + " / " + to_string(len));
				}
				break;
			}
			else
			{	// Socket event detected
				if (FD_ISSET(socket_fd, &write_fd_set))
				{
					packet_bytes_sent += bytes_processed;
#if 0
					{
						FileLog fl(string("client_") + client_ip, false);
						fl.log(string("Sent ") + to_string(packet_bytes_sent) + " / " + to_string(len));
					}
#endif
					F_DBG("%d bytes sent totally", packet_bytes_sent);
				}
			}
			error = 0;
        } while (0);

        if (error)
        {
			F_MSG("Send operation timeout\n");
			{
				FileLog fl(string("client_") + client_ip, false);
				fl.log(string("Send operation timeout"));
			}
			res = EXIT_FAILURE;
			break;
        }
        else
        {
        	res = EXIT_SUCCESS;
        }
	}	// while (packet_bytes_sent < len)
    F_DBG("bytes_processed = %d", bytes_processed);
    return res;
}

static int get_packet(int socket_fd)
{
	int packet_bytes_read = 0;
	int bytes_processed = 0;
	std::vector<char> buffer_in(1024 + ACK_SIZE);

	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

	fd_set read_fd_set;

	F_DBG("get_packet entry..., socket_fd = %d\n", socket_fd);

	do
	{
		int rc;
		struct timeval timeout;
		timeout.tv_sec = CLIENT_ACK_PERIOD_SELECT_OPERATION;
		timeout.tv_usec = 0;

		FD_ZERO(&read_fd_set);
		FD_SET(socket_fd, &read_fd_set);

		rc = select (FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout);
		if (rc < 0)
		{
    		F_DBG("select() failed, errno = %d, %s\n", errno, strerror(errno));
    		continue;
//    		throw(std::runtime_error("select() failed"));
		}

		if (rc == 0)
		{
    		F_DBG("select() timed out\n");
    		throw(std::runtime_error("select() timed out"));
		}

		if (FD_ISSET(socket_fd, &read_fd_set))
		{
			int rc = recv(socket_fd,
					&buffer_in[packet_bytes_read],
					ACK_SIZE - packet_bytes_read,
					0);
			if (rc < 0)
			{
				if (errno != EWOULDBLOCK)
				{
					F_DBG("recv() failed\n");
					throw(std::runtime_error("recv() failed"));
				}
				else
				{
					F_DBG("recv() would block\n");
				}
			} else if (0 == rc)
			{	// Connection is closed
				F_DBG("Connection is closed\n");
				throw(std::runtime_error("Connection is closed"));
				break;
			}
			else
			{	// Data was received
				bytes_processed = rc;
			}
		}
		packet_bytes_read += bytes_processed;
		F_DBG("%d bytes read\n", packet_bytes_read);
	} while ((packet_bytes_read < ACK_SIZE) &&
		(std::chrono::duration_cast <std::chrono::seconds>
			(std::chrono::steady_clock::now() - start).count() < CLIENT_ACK_PERIOD));

	if ((std::chrono::duration_cast < std::chrono::seconds
			> (std::chrono::steady_clock::now() - start).count())
			>= CLIENT_ACK_PERIOD)
	{
		F_MSG("Acknowledge timeout\n");
		throw(std::runtime_error("Acknowledge timeout"));
	}

  F_DBG("bytes_processed = %d", bytes_processed);
  return bytes_processed;
}

static float show_bandwidth(std::shared_ptr<client_data_t> data, std::chrono::steady_clock::time_point& tp_start,
		int packet_nmb, int packet_size, float bytes_sent,
		std::shared_ptr<SharedSemaphore> ss,
		int iter, int iter_max)
{
	int microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - tp_start).count();
	float dt = ((float) microseconds ) / 1000000;
	float bps = (8 * (((float) packet_nmb * packet_size)) / dt) / 1000000;
#ifdef ENABLE_INFO_OUTPUT
	float seconds = (float) microseconds / 1000000;
	F_MSG("%.1f sec  %.1f GBytes  %.2f Mbits/sec  %d packets\n",
			seconds, bytes_sent, bps, packet_nmb);
#endif
	if (bps < (float) MINIMUM_SPEED)
	{
		ss->wait();
		{
			FileLog fl(string("speed_problems"), false);
			fl.log(data->client_ip + (" --> ") + data->ip_str + " - " + to_string((int) bps) + " Mb/s   #" + to_string(iter) +
					" / #" + to_string(iter_max));
		}
		ss->post();
	}

	return bps;
}

class SocketService
{
public:
	SocketService(int domain, int type, int protocol);
	~SocketService();
	int getFd(void);
private:
	int _socket_fd;
};

SocketService::SocketService(int domain, int type, int protocol)
{
	F_MSG("Creating socket...\n");
	_socket_fd = socket(domain, type, protocol);
}

int SocketService::getFd(void)
{
	return _socket_fd;
}

SocketService::~SocketService()
{
	shutdown(_socket_fd, SHUT_RDWR);
	close(_socket_fd);
	F_MSG("Closing socket...\n");
}

void show_status(std::string client_ip, std::string ip_str, int bps, int iter, int iter_max)
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

    if (STATUS_DISCONNECTED == bps)
    	s += ": " + client_ip + " --> " + ip_str + " - " + " disconnect, #" + to_string(iter) + " / " + to_string(iter_max) + "\n";
    else if (STATUS_CONNECTED == bps)
    	s += ": " + client_ip + " --> " + ip_str + " - " + " connected, #" + to_string(iter) + " / " + to_string(iter_max) + "\n";
    else if (STATUS_TIMEOUT == bps)
    	s += ": " + client_ip + " --> " + ip_str + " - " + " timeout, #" + to_string(iter) + " / " + to_string(iter_max) + "\n";
    else
    	s += ": " + client_ip + " --> " + ip_str + " - " + std::to_string(bps) + " Mb/s, #" + to_string(iter) + " / " + to_string(iter_max) + "\n";

	{
		FileLog fl(string("client_status_") + client_ip, false);
		fl.log(s);
	}

	printf("%s", s.c_str());
}

void* client_activity(std::shared_ptr<client_data_t> data, int end_of_session, int iter, int iter_max,
		int &disconnected)
{
	int res = EXIT_SUCCESS;
	int packet_id = 0;
	std::vector<char> buffer_out;
	std::chrono::steady_clock::time_point timepoint_start;

	enum {
		START, MEM_ALLOCATED
	};

	int i;
	struct sockaddr_in sa;
	int ConnectFD;
	int SocketFD;

	F_DBG("Thread %d started\n", data->thread_nmb);

	// Init sockaddr control structure
	memset(&sa, 0, sizeof sa);
	F_DBG("Checkpoint 0.1\n");
	sa.sin_family = AF_INET;
	sa.sin_port = htons(data->port);
	F_DBG("Checkpoint 0.2, data->ip_str = %s\n", data->ip_str.c_str());
	inet_pton(AF_INET, data->ip_str.c_str(), &sa.sin_addr.s_addr);
    // -------------------------------

	SocketService sock(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	{
		FileLog fl(string("client_") + data->client_ip, false);
		fl.log(string("Iteration #") + to_string(iter));
	}

	try
	{
		do  // Once
		{
			int last_packet = 0;
			struct sockaddr_in addr;	//sockaddr_in
			socklen_t len;
			// Create socket

			{
				FileLog fl(string("client_dbg_") + data->client_ip, true);
				fl.log(string("Start"));
			}

			SocketFD = sock.getFd();
			if (-1 == SocketFD)
			{
			  F_MSG("cannot create socket");
			  throw std::runtime_error("Unable to create socket");
			}
			data->socket_fd = SocketFD;

			init_arguments_t controls = {
					.w_flag = 1, .snd_size = data->sbuf_len, .rcv_size = data->rbuf_len
			};
			init_socket(SocketFD, &controls, NULL, NULL); // Get/set sizes of socket SNDBUF, RCVBUF

			F_DBG("Checkpoint 3\n");

			if (0 > set_non_block_flag(SocketFD, 1))  // Make socket non-blocking
			{
				F_MSG("Couldn't set non-blocking socket");
				throw(std::runtime_error("Couldn't set non-blocking socket"));
			}

			// 1. Bind local socket
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = inet_addr(data->client_ip.c_str());	//192.168.1.110
			F_MSG("Client is binding to %s\n", data->client_ip.c_str());
			addr.sin_port = htons(0);
			if (0 != bind(SocketFD, (const sockaddr*) &addr, sizeof(addr)))
			{
				string msg("Cannot bind socket");

				F_MSG("Cannot bind socket\n");
				throw std::runtime_error("Cannot bind socket");
			}

			// 2. Get local socket info
			len = sizeof(addr);
			if (0 != getsockname(SocketFD, (sockaddr*) &addr, &len))
			{
				string msg("Getsockname error");
				F_MSG("Getsockname error");
				throw std::runtime_error("Getsockname error");
			}

			{
				int error = 1;
				do
				{
					struct timeval timeout;
					timeout.tv_sec = CLIENT_WRITE_PACKET_PERIOD;
					timeout.tv_usec = 0;
					int sel_res;

					fd_set write_fd_set;
					ConnectFD = connect(data->socket_fd, (sockaddr*) &sa, sizeof sa);
					if (-1 == ConnectFD)
					{	// Negative value indicates that operation now in progress
						{
							FileLog fl(string("client_") + data->client_ip, false);
							string msg("Connecting, ");
							msg += "errno = " + to_string(errno) + ", " + strerror(errno);
							fl.log(msg);
						}
					}
					else
					{
						FileLog fl(string("client_") + data->client_ip, false);
						fl.log(string("Connect operation is not in progress"));
						break;
					}

					FD_ZERO(&write_fd_set);
					FD_SET(SocketFD, &write_fd_set);

					sel_res = select(FD_SETSIZE, NULL, &write_fd_set, NULL, &timeout);
					if (sel_res < 0)
					{
						FileLog fl(string("client_") + data->client_ip, false);
						string msg("Error on select detected");
						fl.log(msg);

						F_MSG("Select operation error\n");
						break;
					}

					if (FD_ISSET(SocketFD, &write_fd_set))
					{
						FileLog fl(string("client_") + data->client_ip, false);
						string msg("Connection established");
						fl.log(msg);
						if (disconnected)
						{
							show_status(data->client_ip, data->ip_str, STATUS_CONNECTED, iter, iter_max);
							disconnected = 0;
						}
						error = 0;
					}
					else
					{
						FileLog fl(string("client_") + data->client_ip, false);
						string msg("Can't establish a connection");
						fl.log(msg);
					}
				} while(0);

				if (error)
				{
					{
						FileLog fl(string("client_") + data->client_ip, false);
						fl.log(string("Connection failed"));
					}
					std::string msg("Unable to connect");
					F_MSG("Unable to connect\n");
					throw std::runtime_error("Unable to connect");
				}
				else
				{
					{
						FileLog fl(string("client_dbg_") + data->client_ip, false);
						fl.log(string("Connection established"));
					}
				}
			}

			data->connect_fd = ConnectFD;

//			F_MSG("Timestamp: %s", std::asctime(std::localtime(&result)));
			F_MSG("[%d]: local client %s port %d connected with %s, port %d, #%d/#%d\n",
					data->thread_nmb, data->client_ip.c_str(),
					addr.sin_port, data->ip_str.c_str(), data->port, iter, iter_max);

			buffer_out.resize(data->packet_size);

			for (i = 0; i < data->packet_size; i++)
			  buffer_out[i] = i;

			timepoint_start = std::chrono::steady_clock::now();

			F_DBG("\n---------- Sending ----------\n");

			{
				FileLog fl(string("client_dbg_") + data->client_ip, false);
				fl.log(string("Sending"));
			}

			while (!last_packet)
			{
			  if (0 == packet_id)
			  { // Mark first packet
				buffer_out[0] = SOCK_START_TEST;
				{
					FileLog fl(string("client_dbg_") + data->client_ip, false);
					fl.log(string("Sending first packet"));
				}
			  }
			  else if (required_time_passed(data->seconds, timepoint_start))
			  {
				// Mark last packet
				  if (end_of_session)
					  buffer_out[0] = SOCK_CLOSE_COMMAND;
				  else
					  buffer_out[0] = SOCK_SHUTDOWN_COMMAND;
				last_packet = 1;
				{
					FileLog fl(string("client_dbg_") + data->client_ip, false);
					fl.log(string("Sending last packet"));
				}
			  }
			  else
			  { // Mark ordinary packet
				buffer_out[0] = 0;
			  }


			  {
				  FileLog fl(string("client_dbg_") + data->client_ip, false);
				  fl.log(string("Sending packet ") + to_string(packet_id));
			  }

			  if (EXIT_FAILURE == send_packet(data->socket_fd, &buffer_out[0], data->packet_size, data->client_ip, packet_id))
			  {
					FileLog fl(string("client_dbg_") + data->client_ip, false);
					fl.log(string("Error detected"));
					res = EXIT_FAILURE;
					break;
			  }

			  packet_id++;

			  {
				  FileLog fl(string("client_dbg_") + data->client_ip, false);
				  fl.log(string("Packet ") + to_string(packet_id) + " sent");
			  }

			  F_DBG("[%d]: packet %d sent\n", data->thread_nmb, packet_id);
			}
		} while (0);

		if (EXIT_SUCCESS == res)
		{
			// 2. Receive ack
			{
				FileLog fl(string("client_dbg_") + data->client_ip, false);
				fl.log(string("Awaiting ack"));
			}

			F_DBG("\n---------- Awaiting acknowledge ----------\n");

			if (get_packet(data->socket_fd))
			{
				float bytes_sent = ((float) packet_id * data->packet_size) / 1000000000;
				float bps = show_bandwidth(data, timepoint_start, packet_id, data->packet_size, bytes_sent, data->ss, iter, iter_max);
				{
					FileLog fl(string("client_") + data->client_ip, false);
					string msg = string("Bandwidth: ") + to_string((int)bps) + " Mbits/s";
					fl.log(msg);
				}

				show_status(data->client_ip, data->ip_str, (int)bps, iter, iter_max);
			}
			else
			{
				F_INFO("Acknowledge reception failed\n");
			}
		}

		if (0 > close(data->socket_fd))
		{
			FileLog fl(string("client_") + data->client_ip, false);
			string msg = string("Unable to close socket");
			fl.log(msg);
		}
	}
	catch (std::exception &e)
	{
		res = EXIT_FAILURE;
		FileLog fl(string("client_") + data->client_ip, false);
		string msg = string("Exception caught: ") + e.what();
		fl.log(msg);
	}

	{
		FileLog fl(string("client_dbg_") + data->client_ip, false);
		fl.log(string("Leaving client thread"));
	}

	data->result = res;
	return NULL;
}
