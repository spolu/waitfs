#ifndef _SERVER_H
#define _SERVER_H

#define OPEN_CMD "open"          // >> open uid           << ok                     || << error
#define GETLINK_CMD "getlink"    // >> getlink            << readlink ok lid path   || << error
#define SETLINK_CMD "setlink"    // >> setlink lid path   << setlink ok             || << error
#define READLINK_CMD "readlink"  // << readlink lid path
#define OK_CMD "ok"
#define ERROR_CMD "error"

void * start_srv (void * arg);
void * handle (void * arg);

#endif
