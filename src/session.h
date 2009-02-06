#ifndef _HANDLER_H
#define _HANDLER_H

#include <unistd.h>

typedef int sid_t;
typedef int lid_t;

typedef struct cb_arg
{
  sid_t sid;
  sid_t lid;
  void *arg;
} cb_arg_t;

int init_session ();

sid_t session_create (uid_t uid, void *(*readlink_cb)(void *), cb_arg_t *arg);
int session_destroy (sid_t sid);

lid_t session_getlink (sid_t sid);
int session_setlink (sid_t sid, lid_t lid, const char * path);

/*
 * Attempt to read a symlink, blocks until session_setlink is called
 */ 
ssize_t session_readlink (sid_t sid, lid_t lid, char *buf, size_t bufsize);

sid_t * list_sessions ();
lid_t * list_links (sid_t sid);

int session_lock_net (uid_t sid);
int session_unlock_net (uid_t sid);

#endif
