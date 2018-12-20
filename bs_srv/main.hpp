#ifndef __MAIN_HPP__
#define __MAIN_HPP__

typedef struct
{
  int w_flag;
  int rcv_size;
  int snd_size;
  int l_flag;
  int packet_size;
  int n_flag;
  int packets_nmb;
  int b_flag;
  std::string server_ip;
  int c_flag;
  char* ip;
  int p_flag;
  int port_nmb;
  int threads;
  std::string fname;
  std::string client_ip;
  int			i_flag;
  std::string	inaddr;
} command_arguments_t;

#endif
