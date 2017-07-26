/*==============================================================

  MODULE   SOCKETS.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  Module that implements a simple interface to sockets.

================================================================*/

/* Q'Nial file that selects features */

#include "switches.h"

#ifdef SOCKETS

/* standard library header files */

/* IOLIB */
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#ifdef UNIXSYS
#include <sys/mman.h>
#endif
#include <sys/fcntl.h>

/* LIMITLIB */
#include <limits.h>

/* STDLIB */
#include <stdlib.h>

/* STLIB */
#include <string.h>

/* SIGLIB */
#include <signal.h>

/* SJLIB */
#include <setjmp.h>

/* SOCKETLIB */
#ifdef UNIXSYS
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/ip.h>
#  include <netinet/tcp.h>
#  include <netdb.h>
#endif

#ifdef WINNIAL
#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

#endif
#  include <sys/types.h>
#  include <fcntl.h>
#  include <errno.h>


/* Q'Nial header files */

#include "absmach.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"         /* for pair */


#define MEMSET(x,y) memset(x,0,y)
#define LPSOCKADDR struct sockaddr_in *

#ifdef WINNIAL
static void startup_winsock() {
  WORD wVersionRequested;
  WSADATA wsaData;
  static int init = 0;
  int err;

  if (init == 1)
    return;
  init = 1;

  /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
  wVersionRequested = MAKEWORD(2, 2);

  err = WSAStartup(wVersionRequested, &wsaData);
  if (err != 0) {
    /* Tell the user that we could not find a usable */
    /* Winsock DLL.                                  */
    printf("WSAStartup failed with error: %d\n", err);
  }
  return;
}
#endif /* WINNIAL */


/* This is written directly rather than using bzero because
the latter did not load dynamically on one version of SOLARIS.
*/

void mybzero(char *x, int n)
{ unsigned char z = '\0';
  int i;
  for (i=0;i<n;i++)
    *(x+i) = z;
}




/* This call will create a 'server' socket.
 * The arguments are the machine name (string/phrase), followed by
 * a desired port number (integer).  If the port number is zero,
 * then the first free port will be used.
 * The result, on success, will
 * be a pair.  The first will be the resulting socket (integer), and the
 * second will be the port that the socket is bound to (integer).
 */
 
void 
isocket_bind(void)
{
  int rc;
  nialptr z = apop();
  char *hostname;
  int   portnum;
  struct sockaddr_in saddr;
  struct hostent *hp;
  int   thesocket;
  socklen_t   slen;

#ifdef WINNIAL
  startup_winsock();
#endif

  if (tally(z) != 2) {
    freeup(z);
    apush(makefault("bind: must supply a hostname and a portnumber"));
    return;
  }

  if ((kind(fetch_array(z,0)) != chartype) &&
      (kind(fetch_array(z,0)) != phrasetype)) {
    freeup(z);
    apush(makefault("bind: hostname must be a string or phrase"));
    return;
  }
  hostname =  pfirstchar(fetch_array(z,0));

  if (kind(fetch_array(z,1)) != inttype) {
    freeup(z);
    apush(makefault("bind: port number must be a integer"));
    return;
  }
  portnum  = *pfirstint(fetch_array(z,1));

  MEMSET(&saddr,sizeof(saddr));
  saddr.sin_family = AF_INET;

  hp = gethostbyname(hostname);

  if (!hp) {
    apush(makefault("bind: failed to resolve hostname"));
    freeup(z);
    return;
  }
  
  memcpy(&saddr.sin_addr,hp->h_addr,hp->h_length);
  saddr.sin_port = htons((u_short)portnum);

  thesocket = socket(AF_INET,SOCK_STREAM,0);
  if (thesocket < 0) {
    apush(makefault("bind: failed to make the socket"));
    freeup(z);
    return;
  }

  /* avoid problems with re-use of a port */
  {
    int one=1;
    setsockopt(thesocket,SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof(one));
  }

  rc = bind(thesocket,(struct sockaddr *)&saddr, sizeof(saddr));
  if (rc < 0) 
  { char msg[256];
    sprintf(msg,"bind: failed with error code: %d",errno);
    apush(makefault(msg));
    freeup(z);
    return;
  }

  slen = sizeof(struct sockaddr);
  if (getsockname(thesocket, (struct sockaddr *)&saddr, &slen) != 0) {
    apush(makefault("bind: failed to get the socket name"));
    freeup(z);
    return;
  }

  pair(createint(thesocket), createint((int)ntohs(saddr.sin_port)));
  freeup(z);
  return;
}


/**
 * Initiate listening on a socket so that we can accept
 * connections
 */
void
isocket_listen(void)
{
  nialptr z = apop();
  nialint *args;
  int rc;

  if (tally(z) != 2) {
    freeup(z);
    apush(makefault("listen:  must supply portnumber and a queue size"));
    return;
  }
  if (kind(z) != inttype) {
    freeup(z);
    apush(makefault("listen: portnumber and queue size must be integers"));
    return;
  }
  args = pfirstint(z);

  rc = listen(args[0],args[1]);
  if (rc == -1) {
    freeup(z);
    apush(makefault("listen: error returned from listen"));
    return;
  }


  apush(Nullexpr);
  freeup(z);
  return;
}


/**
 * accept an incoming connection
 */
void isocket_accept(void)
{
  nialptr z = apop();
  int arg;
  int fh;

  if (kind(z) != inttype) {
    freeup(z);
    apush(makefault("accept: portnumber must be a integer"));
    return;
  }
  arg = *pfirstint(z);

  fh = accept(arg,NULL,NULL);

  /* If socket is nonblocking then -1 is not an error */
  if (fh == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
    freeup(z);
    apush(makefault("accept: error returned from accept"));
    return;
  }

  apush(createint(fh));
  freeup(z);
  return;

}


/**
 * Initiate a connection to a remote system
 */
void
isocket_connect(void)
{
  int rc;
  nialptr z = apop();
  char *hostname;
  int   portnum;
  struct sockaddr_in saddr;
  struct hostent *hp;
  int   thesocket;

#ifdef WINNIAL
  startup_winsock();
#endif


  if (tally(z) != 2) {
    freeup(z);
    apush(makefault("connect: must supply a hostname and a portnumber"));
    return;
  }

  if ((kind(fetch_array(z,0)) != chartype) &&
      (kind(fetch_array(z,0)) != phrasetype)) {
    freeup(z);
    apush(makefault("connect: hostname must be a string or phrase"));
    return;
  }
  hostname =  pfirstchar(fetch_array(z,0));

  if (kind(fetch_array(z,1)) != inttype) {
    freeup(z);
    apush(makefault("connect: port number  must be a integer"));
    return;
  }
  portnum  = *pfirstint(fetch_array(z,1));

  MEMSET(&saddr,sizeof(saddr));
  saddr.sin_family = AF_INET;

  hp = gethostbyname(hostname);
  if (!hp) {
    apush(makefault("connect: failed to resolve hostname"));
    freeup(z);
    return;
  }

  memcpy(&saddr.sin_addr,hp->h_addr,hp->h_length);
  saddr.sin_port = htons((u_short)portnum);

  thesocket = socket(AF_INET,SOCK_STREAM,0);
  if (thesocket < 0) {
    apush(makefault("connect: failed to make the socket"));
    freeup(z);
    return;
  }

  rc = connect(thesocket,(struct sockaddr *)&saddr, sizeof(saddr));
  if (rc < 0)
  { apush(makefault("connect: failed to connect to the socket"));
    freeup(z);
    return;
  }

  apush(createint(thesocket));
  freeup(z);
  return;
}


/** 
 * Receive a packet from a remote 
 */
void
isocket_receive(void)
{ nialptr z = apop();
 nialptr result;
 nialint number;
 nialint rc;
 int errnokeep;
 int socket, bytecount, startflag;
 int flags = 0;

 if (tally(z) != 3 || kind(z)!=atype) 
 { apush(makefault("?socket_receive: Must supply a portnumber and a byte count"));
   goto cleanup;
 }
 else
 if (kind(fetch_array(z,0)) != inttype) 
 { apush(makefault("?socket_receive: portnumber must be integer"));
   goto cleanup;
 }
 socket = intval(fetch_array(z,0));

 if (kind(fetch_array(z,1)) != inttype) 
 { apush(makefault("?socket_receive: length must be integer"));
   goto cleanup;
 }
 bytecount = intval(fetch_array(z,1));

 if (kind(fetch_array(z,2)) != booltype) 
 { apush(makefault("?socket_receive: startflag must be boolean"));
   goto cleanup;
 }
 startflag = boolval(fetch_array(z,2));

 if (startflag)
 { /* result is the number being received */
   rc = recv(socket,(char *)&number,sizeof(nialint),flags);
   errnokeep = errno;
   if (rc <= 0)
   { if (errnokeep == EWOULDBLOCK)
     { apush(Null); }
     else 
     { char msg[80];
       sprintf(msg,"?socket_receive: connection lost. error %d",errnokeep);
       apush(makefault(msg));
     }
   }
   else
     { apush(createint(number));}
 }
 else
 { /* result is a string of length: bytecount */
   char *data = (char *) malloc(bytecount+1);
   rc = recv(socket,data,bytecount,flags);
   errnokeep = errno;
   if (rc <= 0)
   { if (errnokeep == EWOULDBLOCK)
     { apush(Null); }
     else 
     { char msg[80];
       sprintf(msg,"?socket_receive: connection lost. error %d",errnokeep);
       apush(makefault(msg));
     }
   }
   else
   { /* make sure it is null terminated */
     data[rc] = '\0';
     result = new_create_array(chartype,1,0,&rc);
     memcpy(pfirstchar(result),data,rc);
     apush(result);
   }
   free(data);
 }

cleanup:
  freeup(z);
  return;
}



void
isocket_peek(void)
{ nialptr z = apop();
 nialptr result;
 nialint number;
 nialint rc;
 int errnokeep;
 int socket, bytecount, startflag;
 int flags = MSG_PEEK;
 int fcntl_flags = 0;
    
 if (tally(z) != 3 || kind(z)!=atype) 
 { apush(makefault("?socket_peek: Must supply a portnumber, a byte count and a startflag"));
   goto cleanup;
 }

 if (kind(fetch_array(z,0)) != inttype) 
 { apush(makefault("?socket_peek: portnumber must be integer"));
   goto cleanup;
 }
 socket = intval(fetch_array(z,0));

 if (kind(fetch_array(z,1)) != inttype) 
 { apush(makefault("?socket_peek: length must be integer"));
   goto cleanup;
 }
 bytecount = intval(fetch_array(z,1));

 if (kind(fetch_array(z,2)) != booltype) 
 { apush(makefault("?socket_peek: startflag must be boolean"));
   goto cleanup;
 }
 startflag = boolval(fetch_array(z,2));

#ifdef UNIXSYS
 fcntl_flags = fcntl(socket,F_GETFL,0);
 fcntl_flags |= O_NONBLOCK;
 fcntl(socket,F_SETFL,fcntl_flags);
#endif

 if (startflag)
 { /* result is the number being received */
   rc = recv(socket,(char *)&number,sizeof(nialint),flags);
   errnokeep = errno;
   if (rc <= 0)
   { if (errnokeep == EWOULDBLOCK)
     { apush(Null); }
     else 
     { char msg[80];
       sprintf(msg,"?socket_peek: connection lost. error %d",errnokeep);
       apush(makefault(msg));
     }
   }
   else
     { apush(createint(number));}
 }
 else
 { /* result is a string of length: bytecount */
   char *data = (char *) malloc(bytecount+1);
   rc = recv(socket,data,bytecount,flags);
   errnokeep = errno;
   if (rc <= 0)
   { if (errnokeep == EWOULDBLOCK)
     { apush(Null); }
     else 
     { char msg[80];
       sprintf(msg,"?socket_peek: connection lost. error %d",errnokeep);
       apush(makefault(msg));
     }
   }
   else
   { /* make sure it is null terminated */
     data[rc] = '\0';
     result = new_create_array(chartype,1,0,&rc);
     memcpy(pfirstchar(result),data,rc);
     apush(result);
   }
   free(data);
 }

#ifdef UNIXSYS
    fcntl_flags &= ~O_NONBLOCK;
    fcntl(socket,F_SETFL,fcntl_flags);
#endif

cleanup:
    freeup(z);
    return;
}


void
isocket_send()
{
  nialptr z = apop();
  char * data = NULL;
  int socket, rc, startflag, errnokeep;
  nialptr arr;
  nialint len;


  if (tally(z) != 3 || kind(z)!=atype) {
    freeup(z);
    apush(makefault("?socket_send needs a port, data and start flag"));
    return;
  }

  if (kind(fetch_array(z,0)) != inttype) {
    freeup(z);
    apush(makefault("?socket_send port must be a integer"));
    return;
  }
  socket = *pfirstint(fetch_array(z,0));

  if (kind(fetch_array(z,2)) != booltype) {
    freeup(z);
    apush(makefault("?socket_send flag must be boolean"));
    return;
  }

  startflag = boolval(fetch_array(z,2));

  if (startflag)
  { if (kind(fetch_array(z,1)) != inttype) {
      freeup(z);
      apush(makefault("?socket_send data must be integer at start"));
      return;
    }

    len = intval(fetch_array(z,1));

    rc = send(socket,(char *)&len,sizeof(nialint),0);
    errnokeep = errno;
  }
  else
  {
    if (kind(fetch_array(z,1)) != chartype) {
      freeup(z);
      apush(makefault("socket_send data must be a string"));
      return;
    }

    arr = fetch_array(z,1);

    len = tally(arr);

  
    rc = send(socket,pfirstchar(arr),len,0);
    errnokeep = errno;
   }
   if (rc <= 0) 
   { char msg[80];
     sprintf(msg,"?socket_send: connection lost. error %d",errnokeep);
     apush(makefault(msg));
    free(data);
    freeup(z);
    return;
   }

  apush(Nullexpr);
  freeup(z);
  return;
}


void
isocket_close(void)
{
  nialptr z = apop();
  int socket;

  if (kind(z) != inttype) {
    freeup(z);
    apush(makefault("sclose: portnumber must be a integer"));
    return;
  }
  socket = *pfirstint(z);


  close(socket);
  freeup(z);
  apush(Nullexpr);
  return;
}


/**
 * Disable the Nagle algorithm so that short packets can be sent immediately
 * rather than waiting and bundling them into a larger packet
 */
void isocket_nodelay(void) {
  nialptr x = apop();
  nialptr nfd, nflag;
  int flag, rc;

  if (kind(x) != atype || tally(x) != 2) {
    apush(makefault("?socket_nodelay: args"));
    freeup(x);
    return;
  }

  splitfb(x, &nfd, &nflag);
  if (kind(nfd) != inttype || kind(nflag) != booltype) {
    apush(makefault("?socket_nodelay: arg_types"));
    freeup(x);
    return;
  }

  flag = boolval(nflag);
#ifdef UNIXSYS
  rc = setsockopt(intval(nfd), IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
#endif
#ifdef WINNIAL
  rc = setsockopt(intval(nfd), IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(flag));
#endif

  apush(createint(rc));
  freeup(x);
  return;
}


#endif /* SOCKETS */

