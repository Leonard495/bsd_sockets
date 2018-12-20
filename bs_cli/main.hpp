#ifndef __MAIN_HPP__
#define __MAIN_HPP__

#include <semaphore.h>

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
  int c_flag;
  char* ip;
  int	p_flag;
  int	port_nmb;
  std::string client_ip;
  int	i_flag;
  int	iterations;
  int	t_flag;
  int	time;
} command_arguments_t;

class SharedSemaphore
{
public:
	SharedSemaphore(const char* name);
	~SharedSemaphore();
	void post(void);	// Increments semaphore
	void wait(void);	// Decrements semaphore if it is not zero otherwise blocks and waits for sema to be incremented
private:
	std::string sema_name;
	sem_t *sema;
};

#endif
