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

#define SOCK_PATH "/var/waitfs"

#include "server.h"
#include "session.h"

static pthread_t srv_tid;
char *mount_path;

extern struct fuse_operations waitfs_oper;

int main (int argc, char *argv[])
{  
  struct fuse *fuse;
  int multithreaded;
  int res;

  fuse = fuse_setup(argc, argv, &waitfs_oper, sizeof(waitfs_oper), &mount_path,
			&multithreaded, NULL);
  if (fuse == NULL)
    return 1;
  if (!multithreaded) {
    perror ("fuse needs to be run multithreaded");
    exit (1);
  }

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
  pthread_detach (srv_tid);
  
  //pthread_join (srv_tid, NULL);
  //start_srv ((void *) &sd);

  res = fuse_loop_mt(fuse);

  fuse_teardown(fuse, mount_path);
  if (res == -1)
    return 1;

  return 0;
}

