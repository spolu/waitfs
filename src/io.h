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

#ifndef _PFS_IO_H
#define _PFS_IO_H

#define LINE_BUF_SIZE 512
#define MAX_LINE_BUF_SIZE 8192

#define CRLF "\r\n"
#define LF "\n"

#include <sys/types.h>
#include <unistd.h>

size_t readn (int fd, void *vptr, size_t n);
size_t writen (int fd, const void *vptr, size_t n);
char * readline (int fd);
size_t writeline (int fd, const char *ptr, size_t n, char * ret);

#endif
