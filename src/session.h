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
