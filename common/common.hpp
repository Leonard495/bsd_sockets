#ifndef COMMON_H_
#define COMMON_H_

#define SHARED_SEMA_NAME "/berkeley_sockets_sema"

enum
{
	SERVER_SELECT_ATTEMPTS = 2,
	SEND_ATTEMPTS = 2,
	SERVER_ACK_SEND_ATTEMPTS = 2,
	SERVER_TIMEOUT = 60,					// seconds
	CLIENT_TIMEOUT = SERVER_TIMEOUT + 15,	// seconds
	CLIENT_WRITE_PACKET_PERIOD = 3,
	CLIENT_SEND_PACKET_PERIOD = 3,
	CLIENT_ACK_PERIOD = 2,
	CLIENT_ACK_PERIOD_SELECT_OPERATION = 1,
	SERVER_GET_PACKET_SELECT_TIMEOUT = 3,
	SERVER_SEND_ACK_SELECT_TIMEOUT = 3,
	SERVER_SELECT_ACCEPT_TIMEOUT = 3,
	CLIENT_UNABLE_TO_BIND_MSG_PERIOD = 3,
	MINIMUM_SPEED = 500
};

typedef struct
{
	int		w_flag;
	int		snd_size;
	int		rcv_size;
} init_arguments_t;

int set_non_block_flag(int desc, int value);
void init_socket(int socket_fd, init_arguments_t* cmd, int* sbuf_len, int* rbuf_len);

#endif
