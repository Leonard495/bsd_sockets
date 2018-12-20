#ifndef CLIENTS_H_
#define CLIENTS_H_

#include <string>
#include <thread>

enum
{
	STATUS_TIMEOUT = -3,
	STATUS_CONNECTED = -2,
	STATUS_DISCONNECTED = -1
};

typedef struct
{
  pthread_t         thread_id;
  int               packet_size;
  int               socket_fd;
  int               connect_fd;
  int               seconds;
  int               result;
  std::string       ip_str;
  int               port;
  int               thread_nmb;
  command_arguments_t* args;
  int               sbuf_len;
  int               rbuf_len;
  int               packets_nmb;
  long int          dt;
  float             bps;
  float             bytes_sent;
  char              report[255];
  std::string		client_ip;
  std::shared_ptr<SharedSemaphore> ss;
} client_data_t;


void* client_activity(std::shared_ptr<client_data_t> arg, int end_of_session, int iter, int iter_max, int& disconnected);
void show_status(std::string client_ip, std::string ip_str, int bps, int iter, int iter_max);
#endif
