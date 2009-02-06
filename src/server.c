#include "server.h"
#include "session.h"
#include "io.h"

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

extern char *mount_path;

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

      pthread_create (&last_tid, NULL, &handle, (void *) &cli_sd);
      pthread_detach (last_tid);
      //handle ((void *) &cli_sd);
    }

  return 0;
}


void * handle (void * arg)
{
  sid_t sid;
  lid_t lid;

  char * cmd_line;
  char *cmd_str, *lid_str;
  char * path;

  off_t pos;
  size_t cmd_len;

  int cli_sd = *((int *) arg);

  /*
   * TODO : read the uid
   */
  sid = session_create (0); 
  
  for (;;)
    {
      if ((cmd_line = readline (cli_sd)) == NULL)
	break;

      pos = 0;
      cmd_len = strlen (cmd_line);
            
      for (pos = 0; cmd_line[pos] !=' ' && pos < cmd_len; pos ++);
      cmd_line[pos] = 0;
      cmd_str = cmd_line;
      
      if (strcmp (cmd_str, GETLINK_CMD) == 0) 
	{
	  lid = session_getlink (sid);

	  if (lid > 0) 
	    {
	      char * ret = (char *) malloc (strlen(GETLINK_CMD) +
					    strlen (mount_path) + 128);

	      if (ret == NULL)
		goto error;

	      sprintf (ret, "%s %s %d %s/%d/%d",
		 GETLINK_CMD, OK_CMD, lid, mount_path, sid, lid);
	      writeline (cli_sd, ret, strlen (ret), LF);

	      free (ret);
	      goto done;
	    }
	  else
	    goto error;
	}
      
      else if (strcmp (cmd_str, SETLINK_CMD) == 0) 
	{
	  if (pos + 1 >= cmd_len)
	    goto error;

	  lid_str = cmd_line + (pos + 1);

	  for (;cmd_line[pos] != ' ' && pos < cmd_len; pos ++);
	  cmd_line[pos] = 0; 

	  lid = atoi (lid_str);

	  if (pos + 1 >= cmd_len)
	    goto error;

	  path = cmd_line + (pos + 1);
	  
	  /*
	   * TODO: sanitize path
	   */ 

	  if (session_setlink (sid, lid, path) != 0)
	    goto error;

	  char * ret = (char *) malloc (strlen(SETLINK_CMD) +
					strlen (OK_CMD) + 2);

	  if (ret == NULL)
	    goto error;

	  sprintf (ret, "%s %s", SETLINK_CMD, OK_CMD);
	  writeline (cli_sd, ret, strlen (ret), LF);

	  free (ret);
	}
      
    done:
      if (cmd_line != NULL)
	free (cmd_line);
      continue;

    error:
      writeline (cli_sd, ERROR_CMD, strlen (ERROR_CMD), LF);
      goto done;      
    }  
  session_destroy (sid);

  return NULL;
}
