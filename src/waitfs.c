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

#define SOCK_PATH "/Users/spolu/waitfs"

#include "server.h"
#include "session.h"

static pthread_t srv_tid;
char *mount_path = "/mnt/waitfs";

int main (int argc, char *argv[])
{  
  int sd;
  socklen_t len;
  struct sockaddr_un sun_addr;

  init_session ();

  if ((sd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror ("socket");
    exit (1);
  }

  memset (&sun_addr, 0, sizeof (sun_addr));
  sun_addr.sun_family = AF_UNIX;
  strncpy (sun_addr.sun_path, SOCK_PATH, sizeof (sun_addr.sun_path));

  unlink (sun_addr.sun_path);

  len = strlen (sun_addr.sun_path) + sizeof (sun_addr.sun_family) + 1;
  if (bind (sd, (struct sockaddr *) &sun_addr, len) < 0) {
    perror ("bind");
    exit (1);
  }

  if (listen (sd, 10) < 0) {
    perror ("listen");
    exit (1);
  }

  pthread_create (&srv_tid, NULL, &start_srv, (void *) &sd);
  //pthread_detach (srv_tid);
  
  pthread_join (srv_tid, NULL);

  //start_srv ((void *) &sd);

  //return fuse_main (argc, argv, &test_oper, NULL);
  return 0;
}

