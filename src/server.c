/*  waitfs - symlink wait/update service
 *  Copyright (C) 2009  Stanislas Polu - Ryan Stutsman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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

typedef struct sd
{
  int cli;
  pthread_mutex_t mutex;

} sd_t;

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


void * readlink_cb (void * arg)
{
  sid_t sid;
  lid_t lid;
  sd_t *sd;

  if (arg == NULL)
    return NULL;

  sid = ((cb_arg_t *) arg)->sid;
  lid = ((cb_arg_t *) arg)->lid;
  sd = (sd_t *) ((cb_arg_t *) arg)->arg;

  if (sd == NULL)
    return NULL;

  pthread_mutex_lock (&sd->mutex);
  
  char * ret = (char *) malloc (strlen(READLINK_CMD) +
				strlen (mount_path) + 128);
  
  if (ret == NULL)
    return NULL;
  
  sprintf (ret, "%s %d %s/%d/%d",
	   READLINK_CMD, lid, mount_path, sid, lid);
  writeline (sd->cli, ret, strlen (ret), LF);

  free (ret);
  
  pthread_mutex_unlock (&sd->mutex);
  
  return NULL;
}



void * handle (void * arg)
{
  sid_t sid;
  lid_t lid;

  char *cmd_line;
  char *cmd_str, *lid_str;
  char *path;

  off_t pos;
  size_t cmd_len;

  char userdata[USERDATA_MAX + 1];
  
  sd_t *sd = NULL;
  cb_arg_t *cb_arg = NULL;


  sd = (sd_t *) malloc (sizeof (sd_t));

  if (sd == NULL)
    return NULL;

  sd->cli = *((int *) arg);
  pthread_mutex_init (&sd->mutex, NULL);

  cb_arg = (cb_arg_t *) malloc (sizeof (cb_arg_t));

  if (cb_arg == NULL) {
    free (sd);
    return NULL;
  }
  
  cb_arg->arg = (void *) sd;

  /*
   * TODO : read the uid
   */
  sid = session_create (0, &readlink_cb, cb_arg);
  
  for (;;)
    {
      if ((cmd_line = readline (sd->cli)) == NULL)
	break;

      pthread_mutex_lock (&sd->mutex);

      pos = 0;
      cmd_len = strlen (cmd_line);
            
      for (pos = 0; cmd_line[pos] !=' ' && pos < cmd_len; pos ++);
      cmd_line[pos] = 0;
      cmd_str = cmd_line;
      
      int i;
      for (i = pos; cmd_line[i] != ' ' && i < cmd_len; i++);
      cmd_line[i] = 0;

      strncpy(userdata, cmd_line + pos + 1, USERDATA_MAX);
      userdata[USERDATA_MAX] = '\0';

      if (strcmp (cmd_str, GETLINK_CMD) == 0) 
	{
	  lid = session_getlink (sid);

	  if (lid > 0) 
	    {
	      char * ret = (char *) malloc (strlen(GETLINK_CMD) +
					    strlen (mount_path) + 128);

	      if (ret == NULL)
		goto error;

	      sprintf (ret, "%s %s %s %d %s/%d/%d",
		 GETLINK_CMD, userdata, OK_CMD, lid, mount_path, sid, lid);
	      writeline (sd->cli, ret, strlen (ret), LF);

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

	  sprintf (ret, "%s %s %s", SETLINK_CMD, userdata, OK_CMD);
	  writeline (sd->cli, ret, strlen (ret), LF);

	  free (ret);
	}
      
    done:
      if (cmd_line != NULL)
	free (cmd_line);
      pthread_mutex_unlock (&sd->mutex);
      continue;

    error:
      writeline (sd->cli, ERROR_CMD, strlen (ERROR_CMD), LF);
      goto done;      
    }  

  session_destroy (sid);
  free (cb_arg);
  free (sd);

  return NULL;
}

/* vim: set ts=8 sw=2: */
