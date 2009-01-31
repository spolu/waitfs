#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#include "io.h"

char * getline (void) 
{
  char * line = malloc(100), * linep = line;
  size_t lenmax = 100, len = lenmax;
  int c;
  
  if(line == NULL)
    return NULL;
  
  for(;;) {
    c = fgetc(stdin);
    if(c == EOF)
      break;
    
    if(--len == 0) {
      char * linen = realloc(linep, lenmax *= 2);
      len = lenmax;
      
      if(linen == NULL) {
	free(linep);
	return NULL;
      }
      line = linen + (line - linep);
      linep = linen;
    }
    
    if((*line++ = c) == '\n')
      break;
  }
  *line = '\0';
  return linep;
}

int main(int argc, char ** argv)
{
  int sock;
  struct sockaddr_un server;
  char * buf;
  char * ret;
  
  if (argc < 2) {
    printf("usage:%s <pathname>\n", argv[0]);
    exit(1);
  }
  
  
  sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("opening stream socket");
    exit(1);
  }

  server.sun_family = AF_UNIX;
  strcpy(server.sun_path, argv[1]);  
  
  if (connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) < 0) {
    close(sock);
    perror("connecting stream socket");
    exit(1);
  }

  for (;;)
    {
      buf = getline ();
      writeline (sock, buf, strlen (buf), LF);
      ret = readline (sock);
      printf ("%s\n", ret);
      free (ret);
    }
  
  close(sock);
}

