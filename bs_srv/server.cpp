#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <string.h>	// memset
#include <unistd.h>	// close

#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>

#undef ENABLE_DEBUG_OUTPUT
#undef ENABLE_INFO_OUTPUT
#include <mydef.h>

#include <main.hpp>
#include <flog.hpp>
#include <server.hpp>
#include <common.hpp>

using namespace std;

// Returns 1, if last packet of data has been read
static int get_packet(int connect_fd, char* buf, int packet_size, std::shared_ptr<command_arguments_t> args)
{
	int res = SERVER_NO_COMMAND;
	int packet_bytes_read = 0;
	int timeout_flag = 0;

    {
		FileLog fl(string("server_gp_") + args->server_ip, true);
		fl.log(string("First record"));
    }

	if (!buf)
	{
		F_MSG("Invalid parameter");
		return res;
	}

	while (packet_bytes_read < packet_size)
	{
		int rc;
		fd_set read_fd_set;
		struct timeval timeout;
		timeout.tv_sec = SERVER_GET_PACKET_SELECT_TIMEOUT;
		timeout.tv_usec = 0;
		FD_ZERO(&read_fd_set);
		FD_SET(connect_fd, &read_fd_set);

		rc = select (FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout);
		if (rc < 0)
		{
	    	F_DBG("select() failed, errno = %d, %s\n", errno, strerror(errno));
    		throw(std::runtime_error("select() failed"));
		}

		if (rc == 0)
		{
    		F_DBG("select() timed out\n");
    		throw(std::runtime_error("select() timed out"));
		}

		if (FD_ISSET(connect_fd, &read_fd_set))
		{
		    size_t bytes_received = recv(connect_fd, &buf[packet_bytes_read],
		    		packet_size - packet_bytes_read, 0);
			packet_bytes_read += bytes_received;
			if (0 == bytes_received)
			{
				throw(std::runtime_error("Bytes not found"));
			}
			else
			{
				FileLog fl(string("server_gp_") + args->server_ip, false);
				fl.log(string("Packet received: ") + to_string(bytes_received) + " bytes");
			}
		}
		else
		{
			throw(std::runtime_error("connection socket error"));
		}

		if (timeout_flag)
		{
			res = SERVER_PACKET_TIMEOUT;
		}
		else if (SOCK_SHUTDOWN_COMMAND == buf[0])
		{
			res = SERVER_END_OF_PACKET;
		}
		else if (SOCK_CLOSE_COMMAND == buf[0])
		{
			res = SERVER_END_OF_SESSION;
		}

		F_DBG("packet_bytes_read = %d", packet_bytes_read);
	}
	return res;
}

// Sends a small packet in an answer to client
int send_ack(int connect_fd, char* buf, std::string server_ip, int packet_id)
{
	int packet_bytes_sent = 0;
	int attempts_to_send = SERVER_ACK_SEND_ATTEMPTS;
	int bytes_processed = 0;

	while ((packet_bytes_sent < ACK_SIZE) && (attempts_to_send))
	{
		bytes_processed = send(connect_fd, buf, ACK_SIZE - packet_bytes_sent, MSG_NOSIGNAL);
		if (EPIPE == bytes_processed)
		{
			{
				FileLog fl(string("server_dbg_") + server_ip, false);
				fl.log(string("Write ACK to the broken socket has been detected"));
			}
			break;
		}

		struct timeval timeout;
		int sel_res;
		fd_set write_fd_set;

		FD_ZERO(&write_fd_set);
		FD_SET(connect_fd, &write_fd_set);

		timeout.tv_sec = SERVER_SEND_ACK_SELECT_TIMEOUT;
		timeout.tv_usec = 0;
		sel_res = select(FD_SETSIZE, NULL, &write_fd_set, NULL, &timeout);
		if (sel_res < 0)
		{
			F_MSG("Write select error, send operation timeout");
			FileLog fl(string("server_") + server_ip, false);
			fl.log(string("Write select error, send operation timeout"));
		}
		else if (0 == sel_res)
		{	// Timeout occured
			FileLog fl(string("server_") + server_ip, false);
			fl.log(string("Timeout occured while sending the packet #") + to_string(packet_id) +
				", " + to_string(packet_bytes_sent) +
				", " + to_string(attempts_to_send) + " attempts left");
		}
		else
		{	// Socket event detected
			if (FD_ISSET(connect_fd, &write_fd_set))
			{
				packet_bytes_sent += bytes_processed;
				F_DBG("%d bytes sent totally", packet_bytes_sent);
			}
		}
		attempts_to_send--;
	}

	if (packet_bytes_sent >= ACK_SIZE)
		return 1;
	else
		return 0;
}

static float show_bandwidth(std::chrono::steady_clock::time_point& tp_start, int packet_nmb, int packet_size, float bytes_sent)
{
	int microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - tp_start).count();
	float dt = ((float) microseconds ) / 1000000;
	float bps = (8 * (((float) packet_nmb * packet_size)) / dt) / 1000000;
#ifdef ENABLE_INFO_OUTPUT
	float seconds = (float) microseconds / 1000000;
	F_MSG_SHORT("%.1f sec  %.1f GBytes  %.2f Mbits/sec  %d packets\n",
			seconds, bytes_sent, bps, packet_nmb);
#endif
	return bps;
}

static int server_select_accept_client(int socket_fd, std::shared_ptr<command_arguments_t> args)
{
	fd_set active_fd_set;
	int connect_fd;
	struct timeval timeout;
	std::string error_reason;
	int ready = 0;

	do
	{
		timeout.tv_sec = SERVER_SELECT_ACCEPT_TIMEOUT;
		timeout.tv_usec = 0;

		{
			FileLog fl(string("server_") + args->server_ip, false);
			fl.log(string("Selecting listening socket ") + to_string(socket_fd));
		}

		FD_ZERO(&active_fd_set);
		FD_SET(socket_fd, &active_fd_set);
		int sel_res = select(FD_SETSIZE, &active_fd_set, NULL, NULL, &timeout);
		if (sel_res < 0)
		{
			FileLog fl(string("server_") + args->server_ip, false);
			fl.log(string("Error occurred while selecting socket"));
			error_reason.assign("Error occurred while selecting socket");
			//throw(std::runtime_error("Error occurred while selecting socket"));
			F_MSG("Error occurred while selecting socket");
			break;
		}
		else if (0 == sel_res)
		{
			F_MSG("Server timeout occured on select");
			FileLog fl(string("server_") + args->server_ip, false);
			fl.log(string("Timeout occurred while selecting socket"));
			error_reason.assign("Timeout occurred while selecting socket");
			break;
		}

		{
			FileLog fl(string("server_") + args->server_ip, false);
			fl.log(string("Descriptors to be checked ") + to_string(sel_res));
		}

		if (FD_ISSET(socket_fd, &active_fd_set))
		{
			FileLog fl(string("server_") + args->server_ip, false);
			fl.log(string("Pending connection detected"));
			// Try to accept connection
			connect_fd = accept(socket_fd, NULL, NULL);
			if (0 > connect_fd) {
				FileLog fl(string("server_") + args->server_ip, false);
				fl.log(string("Socket acception failed, attempt #%d"));
				error_reason.assign("Socket acception failed");
				break;
			} else {
				FileLog fl(string("server_") + args->server_ip, false);
				fl.log(string("Socket ") + to_string(connect_fd) + " accepted");
				ready = 1;
			}
		}
		else
		{
			FileLog fl(string("server_") + args->server_ip, false);
			fl.log(string("Unsuspected leave from select") + to_string(sel_res));
			error_reason.assign("Unsuspected leave from select");
		}
	} while (0);

	if (!ready)
	{
		FileLog fl(string("server_") + args->server_ip, false);
		fl.log(string("Connection timeout"));
		F_MSG("Connection timeout");
		std::string message("Server_select_accept_client failed, reason: ");
		message += error_reason;
		throw(std::runtime_error(message.c_str()));
	}

	return connect_fd;
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
}

int server_activity(std::shared_ptr<command_arguments_t> args,
		std::chrono::steady_clock::time_point &successfull_packet_tp,
		std::string& fault_reason)
{
	int res = EXIT_FAILURE;
	int packet_size = args->packet_size;
	struct sockaddr_in sa;
	int SocketFD;
	int ConnectFD;
	int sbuf_len = SNDBUF_SIZE, rbuf_len = RCVBUF_SIZE;

	std::vector<char> buffer_out(ACK_SIZE);
	std::vector<char> buffer_in(packet_size);

	SocketService sock(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	try  //Once
	{
		int end_of_data = SERVER_NO_COMMAND;
		int packet_nmb = 0;

		SocketFD = sock.getFd();
		if (SocketFD < 0)
		{
			{
				FileLog fl(string("server_") + args->server_ip, false);
			  	fl.log(string("Unable to create socket"));
			}
			F_MSG("Unable to create socket");
			throw(std::runtime_error("Unable to create socket"));
		}

	    // Init sockaddr control structure
	    memset(&sa, 0, sizeof sa);
	    sa.sin_addr.s_addr = htonl(INADDR_ANY);
	    sa.sin_family = AF_INET;

	    if (args->p_flag)
	    	sa.sin_port = htons(BASIC_PORT + args->port_nmb);
	    else
	    	sa.sin_port = htons(BASIC_PORT);

	    inet_pton(AF_INET, args->server_ip.c_str(), &sa.sin_addr.s_addr);

	    // Get/set sizes of socket SNDBUF, RCVBUF
	    init_arguments_t controls = {
	    		.w_flag = args->w_flag, .snd_size = args->snd_size,
				.rcv_size = args->rcv_size
	    };

	    init_socket(SocketFD, &controls, &sbuf_len, &rbuf_len);

	    if (0 > set_non_block_flag(SocketFD, 1))  // Make socket non-blocking
	    {
	    	F_MSG("Couldn't set non-blocking socket");
	    	throw(std::runtime_error("Couldn't set non-blocking socket"));
	    }

	    // 2. Bind socket
	    if (-1 == bind(SocketFD, (__CONST_SOCKADDR_ARG) &sa, sizeof sa))
	    {
	    	F_MSG("Unable to bind socket");
	    	throw(std::runtime_error("Unable to bind socket"));
	    }
	    else
	    {
	    	F_MSG("Socket has been binded to %s:%d", args->server_ip.c_str(), BASIC_PORT + args->port_nmb);
	    }

	    if (listen(SocketFD, 1) < 0)
	    {
	    	throw(std::runtime_error("Failed to listen socket"));
	    }

	    ConnectFD = server_select_accept_client(SocketFD, args);
	    F_DBG("Socket connected\n");

	    {
			FileLog fl(string("server_") + args->server_ip, false);
			fl.log(string("Socket ") + to_string(ConnectFD) + " is connected");
	    }
		std::chrono::steady_clock::time_point timepoint_start = std::chrono::steady_clock::now();

		while (SERVER_END_OF_SESSION != end_of_data)
		{
			end_of_data = get_packet(ConnectFD, &buffer_in[0], packet_size, args);
			packet_nmb++;
		    {
				FileLog fl(string("server_gp_") + args->server_ip, false);
				fl.log(string("Packet nmb: ") + to_string(packet_nmb));
		    }
			F_DBG("packet_nmb = %d", packet_nmb);

			if (SERVER_PACKET_TIMEOUT == end_of_data)
			{
			    {
			    	FileLog fl(string("server_") + args->server_ip, false);
			    	fl.log(string("Packet timeout detected"));
			    	F_MSG("Packet timeout detected\n");
			    }
			    throw(std::runtime_error("Server packet timeout"));
			}
			else if (SERVER_END_OF_PACKET == end_of_data)
			{
				// Send acknowledge back from server to client
				F_DBG("\n---------- Sending acknowledge ----------\n");

				if (send_ack(ConnectFD, &buffer_out[0], args->server_ip, packet_nmb))
			    {
					FileLog fl(string("server_") + args->server_ip, false);
					fl.log(string("ACK sent to socket ") + to_string(ConnectFD) + ", closing...");
			    }
				else
				{
					throw(std::runtime_error("Error while sending ACK has been detected"));
				}

				close(ConnectFD);

				F_DBG("--------- Test end ------------\n");

				{
					float bytes_sent = ((double) (packet_nmb * packet_size)) / 1000000000;
					float bps = show_bandwidth(timepoint_start, packet_nmb, packet_size, bytes_sent);
				    {
						FileLog fl(string("server_") + args->server_ip, false);
						fl.log(string("Bandwidth: ") + to_string(bps));
				    }
				}

			    {
			    	FileLog fl(string("server_") + args->server_ip, false);
			    	fl.log(string("Calling server_select_accept_client, socket ") + to_string(SocketFD));
			    }
				ConnectFD = server_select_accept_client(SocketFD, args);
			    {
					FileLog fl(string("server_") + args->server_ip, false);
					fl.log(string("Connection accepted, rw socket ") + to_string(ConnectFD));
			    }

			    timepoint_start = std::chrono::steady_clock::now();
			    packet_nmb = 0;
			    F_DBG("Socket connected\n");
			}
			else if (SERVER_END_OF_SESSION == end_of_data)
			{
				// Send acknowledge back from server to client
				F_DBG("\n---------- Sending acknowledge ----------\n");
			    {
					FileLog fl(string("server_") + args->server_ip, false);
					fl.log(string("Sending acknowledge"));
			    }
				send_ack(ConnectFD, &buffer_out[0], args->server_ip, packet_nmb);
			    {
					FileLog fl(string("server_") + args->server_ip, false);
					fl.log(string("End of test"));
			    }
				if (-1 == shutdown(ConnectFD, SHUT_RDWR))
				{
					throw("Unable to shutdown socket after timeout");
				}
			    close(ConnectFD);

				F_DBG("--------- Test end ------------\n");

				{
					float bytes_sent = (double) (packet_nmb * packet_size) / 1000000000;
					show_bandwidth(timepoint_start, packet_nmb, packet_size, bytes_sent);
				    {
						FileLog fl(string("server_") + args->server_ip, false);
						fl.log(string("End of test"));
				    }
				}
				res = EXIT_SUCCESS;
			}
			else
			{
				successfull_packet_tp = std::chrono::steady_clock::now();
			}
		}

	    {
			FileLog fl(string("server_") + args->server_ip, false);
			fl.log(string("Packets sent: ") + to_string(packet_nmb));
	    }
	} catch (std::exception& e)
	{
		shutdown(ConnectFD, SHUT_RDWR);
	    close(ConnectFD);

		F_MSG("Checkpoint 1");
		FileLog fl(string("server_") + args->server_ip, false);
		fault_reason = std::string(e.what());
		F_MSG("Abnormal situation: %s", e.what());
	}
	F_MSG("Checkpoint 2");
	F_DBG("Thread %d accomplished\n", 0);
	return res;
}
