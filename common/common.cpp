#include <sys/socket.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>

#include <mydef.h>
#include <main.hpp>
#include <common.hpp>

int set_non_block_flag(int desc, int value)
{
  int res;

  do //Once
  {
    int oldflags = fcntl(desc, F_GETFL, 0);
    if (-1 == oldflags)
    {
      res = -1;
      break;
    }
    if (value)
      oldflags |= O_NONBLOCK;
    else
      oldflags &= ~O_NONBLOCK;

    res = fcntl(desc, F_SETFL, oldflags);
  } while (0);
  return res;
}

void init_socket(int socket_fd, init_arguments_t* cmd, int* sbuf_len, int* rbuf_len)
{
  int keep_alive = 0;
  int reuse_addr = 1; // Allow reusing
  int optval;
  socklen_t len;

  if (!cmd)
  {
    F_INFO("Internal error: illegal cmd pointer\n");
    return;
  }

  // 1. SO_KEEPALIVE
  if (-1 == setsockopt(socket_fd,
      SOL_SOCKET,
      SO_KEEPALIVE, &keep_alive, sizeof keep_alive))
  {
    perror("Couldn't change keep alive option: ");
  }

  // 2. SO_REUSEADDR
  if (-1 == setsockopt(socket_fd,
  SOL_SOCKET,
  SO_REUSEADDR, &reuse_addr, sizeof reuse_addr))
  {
    perror("Couldn't set reuse address option: ");
  }
  // 3. SO_SNDBUF
  F_DBG("\nBuffer settings\n");

  if (cmd->w_flag)
    F_DBG("Desired sndbuf_size = %d, desired rcvbuf_size = %d\n",
      cmd->snd_size, cmd->rcv_size);

  len = sizeof(int);
  if (-1 == getsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &optval, &len))
    perror("Couldn't get sndbuf option: ");
  else
    F_DBG("Default sndbuf size = %d\n", optval);

  if (cmd->w_flag)
  {
    if (-1 == setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &cmd->snd_size, sizeof(cmd->snd_size)))
      perror("Couldn't set sndbuf option: ");
  }

  len = sizeof(optval);
  if (-1 == getsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &optval, &len))
    perror("Couldn't get sndbuf option: ");
  else
    F_DBG("New sndbuf size = %d\n", optval);

  if (sbuf_len)
    *sbuf_len = optval;

  // 4. SO_RCVBUF
  len = sizeof(optval);
  if (-1 == getsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &optval, &len))
    perror("Couldn't get rcvbuf option: ");
  else
    F_DBG("Default rcvbuf_size = %d\n", optval);

  if (cmd->w_flag)
  {
    if (-1 == setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &cmd->rcv_size, sizeof(cmd->rcv_size)))
      perror("Couldn't set rcvbuf option");
  }

  len = sizeof(optval);
  if (-1 == getsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &optval, &len))
    perror("Couldn't get rcvbuf option: ");
  else
    F_DBG("rcvbuf size = %d\n", optval);

  if (rbuf_len)
    *rbuf_len = optval;

  F_DBG("---------------\n\n");
}
