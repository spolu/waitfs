#include "server.h"
#include "session.h"

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

static pthread_t last_tid;

void * start_srv (void * arg)
{
  struct sockaddr_un remote;
  socklen_t len;
  int cli_sd;
  int sd = *((int *) arg);

  for(;;) 
    {
      if ((cli_sd = accept(sd, (struct sockaddr *)&remote, &len)) == -1) {
	perror("accept");
	exit(1);
      }

      printf("New Connection.\n");

      pthread_create (&last_tid, NULL, &handle, (void *) &cli_sd);
      pthread_detach (last_tid);
    }

  return 0;
}


void * handle (void * arg)
{
  sid_t sid;
  int cli_sd = *((int *) arg);

  /*
   * TODO : read the uid
   */

  sid = session_create (0); 
  
  while (1)
    {
      /*
       * read command one line
       * parse command
       * check command
       * apply
       * reply one line
       */      
    }  
  
  session_destroy (sid);
}
