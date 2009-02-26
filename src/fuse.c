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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "session.h"

static int waitfs_getattr(const char *path, struct stat *stbuf);
static int waitfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			  off_t offset, struct fuse_file_info *fi);
static int waitfs_readlink (const char *path, char * buf, size_t bufsize);

struct fuse_operations waitfs_oper = {
  .getattr = waitfs_getattr,
  .readdir = waitfs_readdir,
  .readlink = waitfs_readlink
};


static int waitfs_getattr(const char *path, struct stat *stbuf)
{
  sid_t sid = 0;
  lid_t lid = 0;

  char *token, *brkt, *path_copy = NULL;

  printf ("waitfs_getattr %s\n", path);

  memset(stbuf, 0, sizeof(struct stat));

  if(strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }

  path_copy = (char *) malloc (strlen (path) + 1);
  if (path_copy == NULL)
    goto error;

  strncpy (path_copy, path, strlen (path));

  for (token = strtok_r(path_copy, "/", &brkt);
       token;
       token = strtok_r(NULL, "/", &brkt))
    {
      if (sid == 0)
	sid = atoi (token);
      else if (lid == 0)
	lid = atoi (token);
      else
	goto error;
    }
    
  if (sid == 0) {
    goto error;
  }
  else if (lid == 0) 
    {
      uid_t uid = session_getuid (sid);
      if (uid != -1)
      stbuf->st_uid = uid;
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 1;
    }
  else 
    {
      stbuf->st_mode = S_IFLNK | 0755;
      stbuf->st_nlink = 1;
    }

  return 0;

 error:
  if (path_copy != NULL)
    free (path_copy);
  return -ENOENT;
}

static int waitfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			  off_t offset, struct fuse_file_info *fi)
{ 
  sid_t sid = 0;

  sid_t *sessions = NULL;
  sid_t *links = NULL;

  char *token, *brkt, *path_copy = NULL;

  char str[128];

  (void) offset;
  (void) fi;

  printf ("waitfs_readdir\n");

  path_copy = (char *) malloc (strlen (path) + 1);
  if (path_copy == NULL)
    goto error;

  strncpy (path_copy, path, strlen (path));

  for (token = strtok_r(path_copy, "/", &brkt);
       token;
       token = strtok_r(NULL, "/", &brkt))
    {
      if (sid == 0)
	sid = atoi (token);
      else
	goto error;
    }
  
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  
  if (sid == 0) 
    {      
      sessions = list_sessions ();
      
      if (sessions != NULL) {
	for (int i = 0; sessions[i] != 0; i ++) {
	  snprintf (str, 128, "%d", sessions[i]);
	  filler (buf, str, NULL, 0);
	}
      }
    }
  
  else // list links
    {
      links = list_links (sid);

      if (links != NULL) {
	for (int i = 0; links[i] != 0; i ++) {
	  snprintf (str, 128, "%d", links[i]);
	  filler (buf, str, NULL, 0);
	}
      }
    }
  
  return 0;

 error:
  if (path_copy != NULL)
    free (path_copy);
  return -ENOENT;
}


static int waitfs_readlink (const char *path, char * buf, size_t bufsize)
{
  sid_t sid = 0;
  lid_t lid = 0;

  char *token, *brkt, *path_copy = NULL;

  printf ("waitfs_readlink\n");

  path_copy = (char *) malloc (strlen (path) + 1);
  if (path_copy == NULL)
    goto error;

  strncpy (path_copy, path, strlen (path));

  for (token = strtok_r(path_copy, "/", &brkt);
       token;
       token = strtok_r(NULL, "/", &brkt))
    {
      if (sid == 0)
	sid = atoi (token);
      else if (lid == 0)
	lid = atoi (token);
      else
	goto error;
    }
    
  if (sid == 0 || lid == 0) {
    goto error;
  }
  else {
    if (session_readlink (sid, lid, buf, bufsize) < 0)
      goto error;
  }

  return 0;

 error:
  if (path_copy != NULL)
    free (path_copy);
  return -ENOENT;
}
