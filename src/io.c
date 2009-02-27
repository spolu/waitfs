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


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "io.h"


size_t
readn (int fd, void *vptr, size_t n)
{
	ssize_t nleft;
	ssize_t nread;
	char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nread = read (fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
				nread = 0;
			else
				return -1;
		}
		else if (nread == 0)
			break;
		nleft -= nread;
		ptr += nread;
	}
	
	//printf (">> %.*s\n", (n-nleft), vptr);
	return (n - nleft);
}


size_t
writen (int fd, const void *vptr, size_t n)
{
	ssize_t nleft;
	ssize_t nwritten;
	const char *ptr;
	
	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nwritten = write (fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;
			else
				return -1;
		}
		if (nwritten < 0)
			return -1;
		nleft -= nwritten;
		ptr += nwritten;
	}
	return n;
}

const size_t LEN_HDR_LEN = 4;

char *
readline (int fd)
{
	char *line;
	char lenhdr[LEN_HDR_LEN + 1];
	size_t nread;
	long len;
	
	printf ("Waiting for incoming message header\n");
	nread = readn (fd, &lenhdr[0], LEN_HDR_LEN);
	if (nread != LEN_HDR_LEN)
	    {
		printf ("Bad message header length %d\n", nread);
		return NULL;
	    }
	lenhdr[LEN_HDR_LEN] = '\0';
	
	len = strtol (lenhdr, NULL, 10);
	if (!errno)
		return NULL;
	
	printf ("Read message header, following message is %ld bytes\n", len);
	
	if ((line = (char *) malloc ((len + 1) * sizeof (char))) == NULL)
		return NULL;
	
	nread = readn (fd, &line[0], len);
	if (nread != len)
		return NULL;
	line[len] = 0;

	return line;
}

size_t
writeline (int fd, const char *ptr, size_t n, char * ret)
{
	size_t r = 0;
	size_t retlen;
	char lenbuf[LEN_HDR_LEN + 1];
	retlen = strlen(ptr);
	if (retlen >= 10000)
		perror ("return string too long");
	snprintf (lenbuf, LEN_HDR_LEN + 1, "%.4zd", retlen);
	r += writen (fd, lenbuf, LEN_HDR_LEN);
	r += writen (fd, ptr, n);
	return r;
}

/* vim: set ts=8 sw=8 noexpandtab: */
