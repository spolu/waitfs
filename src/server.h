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

#ifndef _SERVER_H
#define _SERVER_H

#define GETLINK_CMD "getlink"    // >> getlink            << readlink ok lid path   || << error
#define SETLINK_CMD "setlink"    // >> setlink lid path   << setlink ok             || << error
#define READLINK_CMD "readlink"  // << readlink lid path
#define SETUID_CMD "setuid"      // >> setuid uid         << setuid ok
#define OK_CMD "ok"
#define ERROR_CMD "error"

#define USERDATA_MAX 10		// maximum size of extra user data

void * start_srv (void * arg);
void * handle (void * arg);

#endif
