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

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>

#include <pthread.h>

#include "session.h"
#include "list.h"
#include "debug.h"

/*
 * Link Data Structure
 */ 
typedef struct link
{
  sid_t sid;                // session id
  lid_t lid;                // link id
  
  char * path;              // path NULL until SETLINK

  uint32_t waiter_cnt;

  struct list_elem elem;
  
  pthread_cond_t cond_set;      
  pthread_mutex_t mutex;

} link_t;



/*
 * Session Data Structure
 */
typedef struct session
{
  sid_t sid;                // session id
  uid_t uid;                // user id

  lid_t last_lid;

  void *(*readlink_cb)(void *);
  cb_arg_t *arg;  

  struct list link_list;    // session links
  
  struct list_elem elem;

} session_t;



/*
 * Sessions list
 */ 
static struct list session_list;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

static int last_sid = 0;

int init_session ()
{
  list_init (&session_list);
  return 0;
}


sid_t session_create (uid_t uid, void *(*readlink_cb)(void *), cb_arg_t *arg)
{
  session_t * new_s = NULL;
  sid_t ret_sid;

  new_s = (session_t *) malloc (sizeof (session_t));

  if (new_s == NULL)
    return -1;

  new_s->sid = ++last_sid;
  new_s->uid = uid;
  new_s->last_lid = 0;

  list_init (&new_s->link_list);

  new_s->readlink_cb = readlink_cb;
  new_s->arg = arg;
  
  ret_sid = new_s->sid;

  pthread_mutex_lock (&session_mutex);
  list_push_front (&session_list, &new_s->elem);
  pthread_mutex_unlock (&session_mutex);

  return ret_sid;
}


int session_destroy (sid_t sid)
{
  struct list_elem *e;
  session_t * session = NULL;
  link_t * link = NULL;

  pthread_mutex_lock (&session_mutex);

  for (e = list_begin (&session_list); e != list_end (&session_list);
       e = list_next (e))
    {
      session_t *s = list_entry (e, session_t, elem);
      if (s->sid == sid) {
	list_remove (e);
	session = s;
      }
    }

  pthread_mutex_unlock (&session_mutex);
  
  if (session == NULL) {
    return 0;
  }

  while (!list_empty (&session->link_list)) 
    {
      e = list_pop_front (&session->link_list);
      link = list_entry (e, link_t, elem);
      
      pthread_mutex_lock (&link->mutex);
      
      if (link->path == NULL) 
	pthread_cond_broadcast (&link->cond_set);		
      
      
      while (link->waiter_cnt > 0) 
	{
	  pthread_mutex_unlock (&link->mutex);      
	  //pthread_yield ();
	  pthread_mutex_lock (&link->mutex);
	}
      
      pthread_mutex_unlock (&link->mutex);      
      
      free (link->path);
      free (link);
    }

  free (session);

  return 0;
}


lid_t session_getlink (sid_t sid)
{
  struct list_elem *e;
  session_t * session = NULL;
  lid_t ret_lid;

  pthread_mutex_lock (&session_mutex);

  for (e = list_begin (&session_list); e != list_end (&session_list);
       e = list_next (e))
    {
      session_t *s = list_entry (e, session_t, elem);
      if (s->sid == sid)
	  session = s;
    }

  if (session == NULL) {
    pthread_mutex_unlock (&session_mutex);
    return -1;
  }

  link_t * new_l = (link_t *) malloc (sizeof (link_t));
  
  new_l->sid = sid;
  new_l->lid = ++session->last_lid;
  new_l->path = NULL;
  new_l->waiter_cnt = 0;
  pthread_mutex_init (&new_l->mutex, NULL);
  pthread_cond_init (&new_l->cond_set, NULL);

  list_push_front (&session->link_list, &new_l->elem);
  
  ret_lid = new_l->lid;

  pthread_mutex_unlock (&session_mutex);
  
  return ret_lid;
}


/*
 * For now session_setlink assumes that path is a valid, checked NULL
 * terminated string
 */
int session_setlink (sid_t sid, lid_t lid, const char * path)
{
  struct list_elem *e;
  session_t * session = NULL;
  link_t * link = NULL;

  pthread_mutex_lock (&session_mutex);

  for (e = list_begin (&session_list); e != list_end (&session_list);
       e = list_next (e))
    {
      session_t *s = list_entry (e, session_t, elem);
      if (s->sid == sid)
	  session = s;
    }

  if (session == NULL) {
    pthread_mutex_unlock (&session_mutex);
    return -1;
  }
  
  for (e = list_begin (&session->link_list); e != list_end (&session->link_list);
       e = list_next (e))
    {
      link_t *l = list_entry (e, link_t, elem);
      if (l->lid == lid)
	  link = l;
    }

  if (link == NULL) {
    pthread_mutex_unlock (&session_mutex);
    return -1;
  }

  pthread_mutex_lock (&link->mutex);
  pthread_mutex_unlock (&session_mutex);

  if (link->path != NULL) {
    pthread_mutex_unlock (&link->mutex);
    return -1;
  }
  
  link->path = (char *) malloc (strlen (path) + 1);

  if (link->path == NULL) {
    pthread_mutex_unlock (&link->mutex);
    return -1;
  }
  
  memset (link->path, 0, strlen (path) + 1);
  strncpy (link->path, path, strlen (path));

  pthread_cond_broadcast (&link->cond_set);
  pthread_mutex_unlock (&link->mutex);

  return 0;
}


ssize_t session_readlink (sid_t sid, lid_t lid, char *buf, size_t bufsize)
{
  struct list_elem *e;
  session_t * session = NULL;
  link_t * link = NULL;
  ssize_t ret_len;

  bool cb_done = 0;
  void *(*cb)(void *);
  cb_arg_t *arg;

 start:
  session = NULL;
  link = NULL;

  pthread_mutex_lock (&session_mutex);

  for (e = list_begin (&session_list); e != list_end (&session_list);
       e = list_next (e))
    {
      session_t *s = list_entry (e, session_t, elem);
      if (s->sid == sid)
	  session = s;
    }

  if (session == NULL) {
    pthread_mutex_unlock (&session_mutex);
    return -1;
  }
  
  for (e = list_begin (&session->link_list); e != list_end (&session->link_list);
       e = list_next (e))
    {
      link_t *l = list_entry (e, link_t, elem);
      if (l->lid == lid)
	  link = l;
    }

  if (link == NULL) {
    pthread_mutex_unlock (&session_mutex);
    return -1;
  }

  if (!cb_done) 
    {
      cb = session->readlink_cb;
      arg = session->arg;
      arg->lid = lid;
      arg->sid = sid;

      pthread_mutex_unlock (&session_mutex);
      
      if (cb != NULL) {
	pthread_t id;
	pthread_create (&id, NULL, cb, (void *) arg);
	pthread_detach (id);
      }
      
      cb_done = true;
      
      goto start;
    }
    
  pthread_mutex_lock (&link->mutex);
  pthread_mutex_unlock (&session_mutex);

  if (link->path == NULL) {
    link->waiter_cnt ++;
    pthread_cond_wait (&link->cond_set, &link->mutex);
    link->waiter_cnt --;
  }
  
  if (link->path == NULL) {
    pthread_mutex_unlock (&link->mutex);
    return -1;
  }

  if (strlen (link->path) >= bufsize-1) {
    pthread_mutex_unlock (&link->mutex);
    return -1;
  }

  memcpy (buf, link->path, bufsize);
  ret_len = strlen (link->path);
  
  pthread_mutex_unlock (&link->mutex);
  
  return ret_len;  
}


sid_t * list_sessions ()
{
  struct list_elem *e;
  size_t size;
  sid_t * ret = NULL;
  off_t pos = 0;

  pthread_mutex_lock (&session_mutex);

  size = list_size (&session_list);

  if (size == 0) {
    pthread_mutex_unlock (&session_mutex);
    return NULL;
  }

  ret = (sid_t *) malloc (sizeof (sid_t) * (size + 1));

  if (ret == NULL) {
    pthread_mutex_unlock (&session_mutex);
    return NULL;
  }
  
  for (e = list_begin (&session_list); e != list_end (&session_list);
       e = list_next (e))
    {
      session_t *s = list_entry (e, session_t, elem);
      ret[pos++] = s->sid;
    }
  
  ret[pos] = 0;

  pthread_mutex_unlock (&session_mutex);
  
  return ret;
}


lid_t * list_links (sid_t sid)
{
  struct list_elem *e;
  session_t * session = NULL;
  lid_t * ret = NULL;
  size_t size;
  off_t pos = 0;

  pthread_mutex_lock (&session_mutex);

  for (e = list_begin (&session_list); e != list_end (&session_list);
       e = list_next (e))
    {
      session_t *s = list_entry (e, session_t, elem);
      if (s->sid == sid)
	  session = s;
    }

  if (session == NULL) {
    pthread_mutex_unlock (&session_mutex);
    return NULL;
  }
  
  size = list_size (&session_list);

  if (size == 0) {
    pthread_mutex_unlock (&session_mutex);
    return NULL;
  }
    

  ret = (sid_t *) malloc (sizeof (sid_t) * (size + 1));
  
  if (ret == NULL) {
    pthread_mutex_unlock (&session_mutex);
    return NULL;
  }

  for (e = list_begin (&session->link_list); e != list_end (&session->link_list);
       e = list_next (e))
    {
      link_t *l = list_entry (e, link_t, elem);
      ret[pos++] = l->lid;
    }

  ret[pos] = 0;

  pthread_mutex_unlock (&session_mutex);
  
  return ret;
}

/* vim: set ts=8 sw=2: */
