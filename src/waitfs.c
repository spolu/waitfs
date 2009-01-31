#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#define FUSE_USE_VERSION 26
#include <fuse.h>

#define SOCK_PATH "/dev/waitfs"

#include "server.h"

static pthread_t srv_tid;

int main (int argc, char *argv[])
{  
  int sd, len;
  struct sockaddr_un local;

  if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }

  local.sun_family = AF_UNIX;
  strcpy(local.sun_path, SOCK_PATH);
  unlink(local.sun_path);

  len = strlen(local.sun_path) + sizeof(local.sun_family);
  if (bind(sd, (struct sockaddr *)&local, len) == -1) {
    perror("bind");
    exit(1);
  }

  if (listen(sd, 10) == -1) {
    perror("listen");
    exit(1);
  }

  pthread_create (&srv_tid, NULL, &start_srv, (void *) &sd);
  pthread_detach (srv_tid);
  
  //return fuse_main (argc, argv, &test_oper, NULL);
  return 0;
}

