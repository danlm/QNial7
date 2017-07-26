/*==============================================================

  MODULE   NSTREAMS.C
 
  COPYRIGHT NIAL Systems Limited  1983-2016

  Contrbuted by John Gibbons

  Binary streams module for Nial

================================================================*/

/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

#ifdef SPROCESS

#ifdef LINUX
#include <pty.h>
#endif

#ifdef OSX
#include <util.h>
#endif

#include <signal.h>

#ifdef LINUX
#include <wait.h>
#endif
#ifdef OSX
#include <sys/wait.h>
#endif
#include <sys/socket.h>

/* standard library header files */

/* IOLIB */
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
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

/* PROCESSLIB */
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

/* TIMELIB  */
#include <time.h>

/* Q'Nial header files */


#include "absmach.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"
#include "nstreams.h"



/* Specify line terminator for system */
static char *eoln_buff = "\n";
static char eoln_len = 1;


/* ======================= Non blocking I/O ========================= */


/**
 * Control non-blocking on the file descriptor
 */
void nio_set_nonblock(int fd, int flag) {
  int fcntl_flags = fcntl(fd, F_GETFL,0);

  if (fd == -1)
    return;

  if (flag) {
    fcntl_flags |= O_NONBLOCK;
  } else {
    fcntl_flags &= ~O_NONBLOCK;
  }

  fcntl(fd, F_SETFL, fcntl_flags);
}


/**
 * Expose non-blocking behaviour for a file descriptor to Nial
 */
void inio_set_nonblock(void) {
  nialptr x = apop();
  nialptr nfd, flag;

  if (kind(x) != atype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  splitfb(x, &nfd, &flag);
  if (kind(nfd) != inttype || kind(flag) != booltype) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }

  /* Call local to perform work */
  nio_set_nonblock((int)intval(nfd), (int)boolval(flag));

  apush(Null);
  freeup(x);
  return;
}


/**
 * Check if we can read from a descriptor. tval controls
 * the time value to wait in microseconds for activity 
 * (-1 is indefinite).
 */
static int isReadable(int fd, nialint tval) {
  fd_set readfds;
  struct timeval timeout;
  int res;

  if (fd == -1)
    return 0;

  for (;;) {
    /* Set the flags */
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    /* Set up for the select call */
    if (tval >= 0) {
      /* set timeout to avoid blocking */
      timeout.tv_sec = tval/1000000;
      timeout.tv_usec = tval % 1000000;
      res = select(fd+1, &readfds, NULL, NULL, &timeout);
    } else {
      /* select with blocking */
      res = select(fd+1, &readfds, NULL, NULL, NULL);
    }
    
    if (res == -1) {
      /* Check for an interrupted call */
      return (errno != EINTR)? -1: 0;
    } else if (res > 0) {
      return FD_ISSET(fd, &readfds);
    } else {
      return 0;
    }
  }
}


/**
 * Check if we can write to a descriptor
 */
static int isWriteable(int fd, nialint tval) {
  fd_set writefds;
  struct timeval timeout;
  int res;

  if (fd == -1)
    return 0;

  for (;;) {
    /* Set the flags */
    FD_ZERO(&writefds);
    FD_SET(fd, &writefds);

    if(tval >= 0) {
      /* set timeout to avoid blocking */
      timeout.tv_sec = tval / 1000000;
      timeout.tv_usec = tval % 1000000;
      
      res = select(fd+1, NULL, &writefds, NULL, &timeout);
    } else {
      res = select(fd+1, NULL, &writefds, NULL, NULL);
    }


    if (res == -1) {
      return (errno != EINTR)? -1: 0; 
    } else if (res > 0) {
      return FD_ISSET(fd, &writefds);
    } else {
      return 0;
    }
  }
}



/* ============================ Buffering ============================ */

static SP_BuffPtr sp_freelist = NULL;


static SP_BuffPtr createBuffer() {
  if (sp_freelist != NULL) {
    SP_BuffPtr b = sp_freelist;
    sp_freelist = sp_freelist->next;
    b->count = 0;
    b->d_start = 0;
    b->d_end = 0;
    b->next = NULL;
    return b;
  } else {
    SP_BuffPtr b = (SP_BuffPtr)malloc(sizeof(SP_Buffer));

    if (b != NULL) {
      b->count = 0;
      b->d_start = 0;
      b->d_end = 0;
      b->next = NULL;
    }

    return b;
  }
}


/**
 * add a buffer to the freelist 
 */
static void freeBuffer(SP_BuffPtr b) {
  if (sp_freelist != NULL) {
    b->next = sp_freelist;
    sp_freelist = b;
  } else {
    sp_freelist = b;
    b->next = NULL;
  }
}


/**
 * Pack buffer to front 
 */
static void packBuffer(SP_BuffPtr b) {
  if (b->d_start > 0) {
    if (b->count > 0) {
      unsigned char *data = b->buff;
      memmove(data, data+b->d_start, b->count);
    }
    b->d_start = 0;
    b->d_end = b->count;
  }
}


/* ====================== Streams ============================ */



/**
 * Create a new stream structure 
 */
static SP_StreamPtr createStream() {
  SP_StreamPtr s = (SP_StreamPtr)malloc(sizeof(SP_Stream));
  if (s != NULL) {
    SP_BuffPtr b = createBuffer();
    if (b != NULL) {
      s->index  = -1;
      s->fd     = -1;
      s->mode   = IOS_INTERNAL;
      s->status = IOS_NORMAL;
      s->flags  = IOS_NOFLAGS;
      s->count  = 0;
      s->first  = b;
      s->last   = b;
    } else {
      free(s);
      s = NULL;
    }
  }

  return s;
}
    

/**
 * Free up a stream structure
 */
void nio_freeStreamPtr(SP_StreamPtr s) {
  SP_BuffPtr b = s->first;
  while (b != NULL) {
    SP_BuffPtr n = b->next;
    freeBuffer(b);
    b = n;
  }
  free(s);
}


/**
 * Append a block of characters/bytes to the end of a stream.
 * The stream will be extended if necessary.
 */
static int appendChars(SP_StreamPtr stream, unsigned char *buff, nialint len) {
  nialint nbytes = len;
  unsigned char *p = buff;

  while(nbytes > 0) {
    SP_BuffPtr bp = stream->last;
    int space = SP_BUFFSIZE - bp->count;

    /* move space to end if not already there*/
    if (bp->d_start != 0)
      packBuffer(bp);     

    if (space == 0) {
      /* No room left */
      SP_BuffPtr n = createBuffer();
	
      if(n == NULL)
	return -1;
      
      bp->next = n;
      stream->last = n;
      n->next = NULL;
      
    } else {
      /* room for chars */
      int nc = (nbytes > space) ? space: nbytes;
      
      memcpy(bp->buff+bp->d_end, p, nc);
      p += nc;
      bp->count += nc;
      bp->d_end += nc;
      nbytes -= nc;
      stream->count += nc;
    }
    
  }

  return 0;
}


/**
 * Remove a block of characters/bytes from the beginning of a stream.
 * Empty buffers will be removed from the stream.
 */
static nialint nio_getchars(SP_StreamPtr stream, unsigned char *buff, nialint len) {
  nialint nbytes = (len > stream->count)? stream->count: len;
  nialint res = nbytes;
  unsigned char *p = buff;

  while(nbytes > 0) {
    SP_BuffPtr bp = stream->first;

    if (bp->count == 0) {
      /* Empty buffer, remove */
      stream->first = bp->next;
      freeBuffer(bp);
          
    } else {
      /* chars in buffer */
      int nc = (nbytes > bp->count)? bp->count: nbytes;

      memcpy(p, bp->buff+bp->d_start, nc);
      p += nc;
      bp->count -= nc;
      bp->d_start += nc;
      nbytes -= nc;
      stream->count -= nc;
    }
    
  }
  
  return res;
}


/**
 * Find a newline in a stream. This will return the number of characters/bytes in
 * the line or 0 if no line found.
 */
static nialint findLine(SP_StreamPtr stream) {
  nialint count = 0;
  int found = 0;

  SP_BuffPtr b = stream->first;

  while (!found && b != NULL) {
    unsigned char *p = b->buff+b->d_start;
    int i;

    for (i = 0; i < b->count; i++) {
      count++;
      if ((0xFF & *p++) == '\n') 
	return count;
    }

    b = b->next;
  }

  return 0;
}

/* ======================= Streams ============================ */

#define MAX_STREAMS 4096
static SP_StreamPtr nio_streams[MAX_STREAMS];
static int stream_init = 0;



#define VALID_STREAM(i) (stream_init == 1 && 0 <= i && i < MAX_STREAMS && nio_streams[i] != NULL)


SP_StreamPtr nio_stream_ptr(int ios) {
  return nio_streams[ios];
}


/**
 * Create and register a stream object
 */
int nio_createStream() {
  SP_StreamPtr ios = createStream();
  int i;

  if (ios == NULL)
    return -1;

  /* Initialise the system if needed */
  if (!stream_init) {
    for (i = 0; i < MAX_STREAMS; i++)
      nio_streams[i] = NULL;
    stream_init = 1;
  }

  /* Find a free slot */
  for (i = 0; i < MAX_STREAMS; i++) {
    if (nio_streams[i] == NULL) {
      ios->index = i;
      nio_streams[i] = ios;
      return i;
    }
  }

  return -1;
}


/** 
 * Set the file descriptor for a stream 
 */
int nio_set_fd(int ios, int fd) { 
  if (VALID_STREAM(ios)) {
    nio_streams[ios]->fd = fd;
    return fd;
  } else {
    return -1;
  }
}


/** 
 * Get the file descriptor for a stream 
 */
int nio_get_fd(int ios) { 
  if (VALID_STREAM(ios)) {
    return nio_streams[ios]->fd;
  } else {
    return -1;
  }
}


/**
 * Return the descriptor associated with a stream
 */
void inio_get_fd(void) {
  nialptr x = apop();
  
  if (kind(x) == inttype && VALID_STREAM(intval(x))) {
    apush(createint(nio_get_fd(intval(x))));
    freeup(x);
    return;
  } else {
    apush(makefault("?invalid_stream"));
    freeup(x);
    return;
  }
}


/** 
 * Set the mode for a stream 
 */
int nio_set_mode(int ios, int mode) { 
  if (VALID_STREAM(ios)) {
    nio_streams[ios]->mode = mode;
    return mode;
  } else {
    return -1;
  }
}



/** 
 * Set the flags for a stream 
 */
int nio_set_flags(int ios, int flags) { 
  if (VALID_STREAM(ios)) {
    nio_streams[ios]->flags = flags;
    return flags;
  } else {
    return -1;
  }
}


/**
 * Clear out a stream
 */
void nio_freeStream(int ios) {
  if (VALID_STREAM(ios)) {
    SP_StreamPtr sp = nio_streams[ios];
    if (sp->fd != -1)
      close(sp->fd);
    nio_freeStreamPtr(sp);
    nio_streams[ios] = NULL;
  }
}


/**
 * Write as much data from a buffer to a descriptor as is 
 * possible. Update the pointer and buffer count.
 */ 
static int writeBuffer(int fd, SP_BuffPtr bp) {
  unsigned char *p = bp->buff + bp->d_start;
  int nw;
  
  nw = write(fd, p, bp->count);
  if (nw > 0) {
    p += nw;
    bp->d_start += nw;
    bp->count -= nw;
  }
  
  return nw;
}


/**
 * Write from the stream to the associated file. Returns a 
 * count of the number of characters written. The flag value
 * is a wait timer on polls.
 *
 * This routine will write as much as possible to the file
 * descriptor but may not write the whole stream if it is
 * interrupted or an error occurs.
 */
static nialint writeStream(SP_StreamPtr ios, int flag) {
  nialint nch, count = 0;
  int fd = ios->fd;

  if (fd == -1)
    return 0;

  while(ios->count > 0) {
    SP_BuffPtr bp = ios->first;
    int poll = isWriteable(fd, flag);

    if (poll > 0) {
      /* Ok to write */
      nch = writeBuffer(fd, bp);
      if (nch < 0) {
	/* Write failed for some reason */
	switch (errno) {
	case EAGAIN:
#ifdef EAGAIN_IS_NOT_EWOULDBLOCK
	case EWOULDBLOCK:
#endif
	case EINTR:
	  return count;
	default:
	  ios->status = IOS_EOF;
	  return -1;
	}
      } else if (nch == 0) {
	ios->status = IOS_EOF;
	return count;
      } else {
	count += nch;
	ios->count -= nch;
	if (bp->count == 0 && bp != ios->last ) {
	  ios->first = bp->next;
	  freeBuffer(bp);
	}
      }
    } else if (poll == 0) {
      /* Not able to write */
      return count;
    } else {
      /* Some error occured */
      return (errno == EINTR)? count: -1;
    }
  }

  return count;
}


/**
 * nio_write_stream <stream> <flags>
 */
void inio_write_stream(void) {
  nialptr x = apop();
  nialint *iptr, ios, flags, count;

  if (kind(x) != inttype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  iptr = pfirstint(x);
  ios = *iptr++;
  flags = *iptr;

  if (!VALID_STREAM(ios)) {
    apush(makefault("?invalid_stream"));
    freeup(x);
    return;
  }
  
  count = writeStream(nio_streams[ios], flags);
  if (count < 0) {
    apush(makefault("?nio_error"));
  } else {
    apush(createint(count));
  }

  freeup(x);
  return;
}

  

/**
 * Poll a stream for input.
 *
 * The climit parameter indicates how many characters are desired
 * by the caller. A value of -1 indicates there is no desired value.
 * Once climit is reached the call returns.
 * 
 * The flag value controls the behaviour of the routine using timeouts.
 * If the flag is -1 then we block on input, reading until we have
 * enough input for climit.
 *
 * If flag is not -1 then we do not block on input. We read whatever is
 * available.
 *
 * The routine always returns the number of characters available.
 *
 */
static nialint poll_input(SP_StreamPtr sp, nialint climit, nialint flag) {
  int poll;
  nialint nch;
  unsigned char buff[SP_BUFFSIZE];

  /* If there is no descriptor or we are at end of file do nothing */
  if (sp->fd == -1 || sp->status == IOS_EOF)
    return sp->count;

  /* If we already have the required number then return */
  if (climit != -1 && sp->count >= climit) {
    return sp->count;
  }

  for (;;) {
    poll = isReadable(sp->fd, flag);
    if (poll > 0) {
      nch = read(sp->fd, buff, SP_BUFFSIZE);
      
      if (nch == -1) {
        if (errno != EINTR) {
          sp->status = IOS_EOF;
          return sp->count;
        }
      } else if (nch == 0) {
        sp->status = IOS_EOF;
        return sp->count;
      } else {
        appendChars(sp, buff, nch);
        if (climit != -1 && sp->count >= climit)
          return sp->count;
      }
    } else { 
      return sp->count;
    }
  }
}


/**
 * count := nio_read_stream ios climit flags
 */
void inio_read_stream(void) {
  nialptr x = apop();
  nialint *iptr, ios, climit, flags;

  if (kind(x) != inttype || tally(x) != 3) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  iptr = pfirstint(x);
  ios = *iptr++;
  climit = *iptr++;
  flags = *iptr++;

  if (!VALID_STREAM(ios)) {
    apush(makefault("?invalid_stream"));
    freeup(x);
    return;
  }

  apush(createint(poll_input(nio_streams[ios], climit, flags)));
  freeup(x);
  return;
}


/**
 * create a stream for Nial use
 */
void inio_openStream(void) {
  nialptr x = apop();
  nialint *iptr;
  nialint s;
  
  if (kind(x) != inttype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  iptr = pfirstint(x);
  s = nio_createStream();
  if (s == -1) {
    apush(makefault("?nomem"));
    freeup(x);
    return;
  }

  nio_streams[s]->fd = iptr[0];
  nio_streams[s]->mode = iptr[1];
  apush(createint(s));
  freeup(x);
  return;
}


/**
 * Close a stream
 */
void inio_closeStream(void) {
  nialptr x = apop();

  if(kind(x) != inttype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  nio_freeStream(intval(x));
  apush(True_val);
  freeup(x);
  return;
}


#ifdef NSTREAMS_DUMP_BUFFER
static void dumpBuffer(SP_BuffPtr bp) {
  nialint *iptr;
  nialint count = bp->count;

  
  packBuffer(bp);

  iptr = (nialint*)(bp->buff+bp->d_start);

  printf("Count: %ld\n", count);
  printf("----------------\n\n");

  while (count > 0) {
    int i;
    for (i = 0; i < 4; i++) {
      printf(" %16lx", *iptr);
      printf(" %16ld", *iptr++);
    }
    count -= 4*sizeof(nialint);
    printf("\n");
  }

  printf("\n");
}


static void dumpStream(SP_StreamPtr sp) {
  SP_BuffPtr bp;

  for(bp = sp->first; bp != NULL; bp = bp->next) 
    dumpBuffer(bp);
}
#endif


/**
 * Append characters to the stream
 */
void inio_write(void) {
  nialptr x = apop();
  nialptr st, buf;
  nialint io;
  SP_StreamPtr sp;

  if (kind(x) != atype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  splitfb(x, &st, &buf);
  if (kind(st) != inttype || kind(buf) != chartype) {
   apush(makefault("?args"));
    freeup(x);
    return;
  }

  io = intval(st);
  if (!VALID_STREAM(io)) {
    apush(makefault("?invalid_stream"));
    freeup(x);
    return;
  }

  sp = nio_streams[io];
  appendChars(sp, (unsigned char *)pfirstchar(buf), tally(buf));

  apush(createint(tally(buf)));
  freeup(x);
  return;
}


/**
 * Append characters to the stream
 */
void inio_writeln(void) {
  nialptr x = apop();
  nialptr st, buf;
  nialint io;
  SP_StreamPtr sp;

  if (kind(x) != atype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  splitfb(x, &st, &buf);
  if (kind(st) != inttype || kind(buf) != chartype) {
   apush(makefault("?args"));
    freeup(x);
    return;
  }

  io = intval(st);
  if (!VALID_STREAM(io)) {
    apush(makefault("?invalid_stream"));
    freeup(x);
    return;
  }

  /* append all chars to the stream */
  sp = nio_streams[io];
  appendChars(sp, (unsigned char *)pfirstchar(buf), tally(buf));
  appendChars(sp, (unsigned char *)eoln_buff, eoln_len);

  /* Return a count */
  apush(createint(tally(buf)+eoln_len));
  freeup(x);
  return;
}


/**
 * Return a count of the characters in the stream
 */
void inio_count(void) {
  nialptr x = apop();
  nialint ios;
  
  if (kind(x) != inttype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  ios = intval(x);
  if (!VALID_STREAM(ios)) {
    apush(makefault("?invalid_stream"));
    freeup(x);
    return;
  }

  apush(createint(nio_streams[ios]->count));
  freeup(x);
  return;
}


/**
 * read a line from the stream if one exists
 */
void inio_readln(void) {
  nialptr x = apop();
  nialint ios;
  
  if (kind(x) != inttype || !VALID_STREAM(intval(x))) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  ios = intval(x);
  if (poll_input(nio_streams[ios], IOS_NOLIMIT, 1) > 0) {
    SP_StreamPtr sp = nio_streams[ios];
    nialint nch = findLine(sp);
  
    if (nch <= 0) {
      apush(Null);
      freeup(x);
      return;
    } else {
      nialptr res = new_create_array(chartype, 1, 0, &nch);
      nio_getchars(sp, (unsigned char*)pfirstchar(res), nch);
      apush(res);
      freeup(x);
      return;
    }
  } else {
    apush(Null);
    freeup(x);
    return;
  }
}


void inio_read(void) {
  nialptr x = apop();
  nialint ios;
  nialint scount, nch;
  nialint *iptr;
  nialptr res = Null;

  if (kind(x) != inttype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  iptr = pfirstint(x);
  ios = *iptr++;
  nch = *iptr++;
  
  if (!VALID_STREAM(ios) || nch <= 0) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  res = Null;
  if ((scount = poll_input(nio_streams[ios], nch, 0)) > 0) {
    SP_StreamPtr sp = nio_streams[ios];
    nch = (nch > scount)? scount: nch;
    res = new_create_array(chartype, 1, 0, &nch);
    nio_getchars(sp, (unsigned char *)pfirstchar(res), nch);
  }
  
  apush(res);
  freeup(x);
  return;
}


/**
 * Return the current status of the file
 */
void inio_status(void) {
  nialptr x = apop();
  nialint fstatus;
  SP_StreamPtr sp;
  
  
  if (kind(x) != inttype || !VALID_STREAM(intval(x))) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  /* 
   * Determine the status by first looking at the count. If there
   * are characters in the buffer then treat that as normal
   * otherwise take the status value.
   */ 
  sp = nio_streams[intval(x)];
  if (sp->count > 0)
    fstatus = IOS_NORMAL;
  else
    fstatus = sp->status;

  apush(createint(fstatus));
  freeup(x);
  return;
}



/**
 * Serialise an array to a stream
 */
static int
block_array(SP_StreamPtr sp, nialptr x)
{
  nialint     t,
              v,
              i,
              n;
  nialint         k;


  /* write the kind field */
  k = kind(x);
  if (appendChars(sp, (unsigned char *) &k, sizeof(nialint)) < 0) {
    return -1;
  }

  /* write the valence */
  v = valence(x); 
  if (appendChars(sp, (unsigned char *) &v, sizeof(nialint)) < 0) {
    return -1;
  }


  /* write the tally or string length of a phrase or fault */
  if (k == phrasetype || k == faulttype)
    t = strlen(pfirstchar(x));
  else
    t = tally(x);

  if (appendChars(sp, (unsigned char *) &t, sizeof(nialint)) < 0) {
    return -1;
  }

  /* write the shape vector */ 
  if (v>0) { 
    if (appendChars(sp, (unsigned char *) shpptr(x, v), v * sizeof(nialint)) < 0) {
      return -1;
    }
  }

  /* compute size for each atomic type and write it */
  if (k != atype) { 
    switch (k) {
      case phrasetype:
      case faulttype:
          n = tknlength(x) + 1;
          break;
      case booltype:
          n = (t / boolsPW + ((t % boolsPW) == 0 ? 0 : 1)) * sizeof(nialint);
          break;
      case chartype:
          n = t + 1;         /* leave space for terminating null */
          break;
      case inttype:
          n = t * sizeof(nialint);
          break;
      case realtype:
          n = t * sizeof(double);
          break;
#ifdef COMPLEX
      case cplxtype:
          n = t * 2 * sizeof(double);
          break;
#endif
    }

    /* Write the size */
    if (appendChars(sp, (unsigned char *) &n, sizeof(nialint)) < 0) {
      return -1;
    }

    /* write the atomic data */
    if (appendChars(sp, (unsigned char *) pfirstitem(x), n) < 0) {
      return -1;
    }

  } else { 
    /* write the array blocks for each of the items recursively */
    for (i = 0; i < t; i++) 
      if (block_array(sp, fetch_array(x, i)) < 0)
	return -1;

  }

  /* dumpStream(sp); */

  return 0;
}


/** 
 * Unpacks an array as it reads it in from a stream.
 * This assumes that the array is stored completely in the stream.
 * That is handled by the caller.   
 */
static      nialptr
unblock_array(SP_StreamPtr sp)
{
  static nialint tmp;
  nialint     t,
    v,
              i,
              n,
              tknsize;
  nialptr     x = invalidptr,
              sh = -1;
  nialint         k;

  /* read the kind of array */
  if (poll_input(sp, sizeof(nialint), -1) < sizeof(nialint)) {
    return invalidptr;
  }
  nio_getchars(sp, (unsigned char *)&tmp, sizeof(nialint));
  k = tmp;

  /* read the valence */
  if (poll_input(sp, sizeof(nialint), -1) < sizeof(nialint)) {
    return invalidptr;
  }
  nio_getchars(sp, (unsigned char *)&tmp, sizeof(nialint));
  v = tmp;

  /* read the tally or string length of a phrase or fault */
  if (poll_input(sp, sizeof(nialint), -1) < sizeof(nialint)) {
    return invalidptr;
  }
  nio_getchars(sp, (unsigned char *)&tmp, sizeof(nialint));
  t = tmp;

  /* create the spare vector and fill it if valence > 0 */
  sh = new_create_array(inttype, 1, 0, &v);
  if (v>0) {
    nialint shlen = v*sizeof(nialint);
    if (poll_input(sp, shlen, -1) < shlen) {
      freeup(sh);
      return invalidptr;
    } else {
      nio_getchars(sp, (unsigned char *)(pfirstint(sh)), shlen);
    }
  }
  
  /* create x as the container for the result if not a phrase or fault */
  tknsize = (k == phrasetype || k == faulttype ? t : 0);
  if (k != phrasetype && k != faulttype) {
    x = new_create_array(k, v, tknsize, pfirstint(sh)); /* invalid shape is ok */
  }
  freeup(sh);

  if (k != atype) { 
    /* filling an atomic or homogeneous array */
    /* read the number of items */

    if (poll_input(sp, sizeof(nialint), -1) < sizeof(nialint)) {
      if (x != invalidptr) freeup(x);
      return invalidptr;
    } else {
      nio_getchars(sp, (unsigned char *) &tmp, sizeof(nialint));
      n = tmp;
    }
    
    if (k == phrasetype) { /* create the phrase */
      if (poll_input(sp, n, -1) < n) {
	return invalidptr;
      } else {
	nio_getchars(sp, (unsigned char *) gcharbuf, n);
	x = makephrase(gcharbuf); /* to check for uniqueness */
      }

    } else if (k == faulttype) { /* create the fault */
      if (poll_input(sp, n, -1) < n) return invalidptr;
      nio_getchars(sp, (unsigned char *) gcharbuf, n);
      x = makefault(gcharbuf);  /* to check for uniqueness */

    } else { /* fill x as a homogeneous array */
      if (poll_input(sp, n, -1) < n) {
	if (x != invalidptr) freeup(x);
	return invalidptr;
      } else {
	nio_getchars(sp, (unsigned char *) pfirstchar(x), n);
      }
    }

  } else { /* fill x as atype using recursion to fill each item. */
    for (i = 0; i < t; i++) {
      nialptr recur = unblock_array(sp);
      store_array(x, i, recur);
    }
  }

  return (x);

}


void inio_unblock_array(void) {
  nialptr x = apop();
  nialint ios;
  SP_StreamPtr sp;
  nialptr res;

  if (kind(x) != inttype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  ios = intval(x);
  if (!VALID_STREAM(ios)) {
    apush(makefault("?invalid_stream"));
    freeup(x);
    return;
  }

  sp = nio_streams[ios];
  res = unblock_array(sp);

  if (res == invalidptr) {
    sp->status = IOS_EOF;
    apush(Null);
  } else {
    apush(res);
  }

  freeup(x);
  return;
}


void inio_block_array(void) {
  nialptr x = apop();
  nialptr s, a;
  SP_StreamPtr sp;


  if ((kind(x) != atype && kind(x) != inttype) || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  if (kind(x) == atype) {
    splitfb(x, &s, &a);
    if (kind(s) != inttype || !VALID_STREAM(intval(s))) {
      apush(makefault("?args"));
      freeup(x);
      return;
    }
    
    sp = nio_streams[intval(s)];
  } else {
    nialint *iptr = pfirstint(x);
    
    if (!VALID_STREAM(*iptr)) {
      apush(makefault("?invalid_streams"));
      freeup(x);
      return;
    }
    
    sp = nio_streams[*iptr++];
    a = createint(*iptr++);
  }

  if (block_array(sp, a) < 0) { 
    apush(False_val);
  } else {
    apush(True_val);
  }
  freeup(x);
  return;
}


/**
 * fill an fdset from the input stream list
 */
static int make_poll_fdset(fd_set *fds, nialptr n_fds) {
  int fd, selcnt = -1;
  nialint *iptr, nc, i, ios;

  /* Handle a null case */
  if (n_fds == Null)
    return selcnt;
  
  /* Set up the read set */
  iptr = pfirstint(n_fds);
  nc = tally(n_fds);
  for (i = 0; i < nc; i++) {
    ios = iptr[i];
    if (VALID_STREAM(ios)) {
      fd = (nio_streams[ios])->fd;
      if (fd != -1) {
        selcnt = (fd > selcnt)? fd: selcnt;
        FD_SET(fd, fds);
      }
    }
  }
  
  return selcnt;
}


/**
 * Create a boolean array to match the fdset
 */
static nialptr make_poll_result(fd_set *fds, nialptr n_fds) {
  nialptr nres;
  nialint i, ios;
  nialint *iptr, nfds;
  int fd;

  /* If no descriptors just return null */
  if (n_fds == Null)
    return Null;

  /* Create a return array */
  iptr = pfirstint(n_fds);
  nfds = tally(n_fds);
  nres = new_create_array(booltype, 1, 0, &nfds); 

  /* fill from the fdset */
  for (i = 0; i < nfds; i++) {
    ios = iptr[i];
    if (VALID_STREAM(ios)) {
      fd = (nio_streams[ios])->fd;
      if (fd != -1) {
        /* Valid fd, check the set */
        store_bool(nres, i, FD_ISSET(fd, fds)? 1: 0);
      } else {
        /* fd is -1, readable if data in stream */
        store_bool(nres, i, 0);
      }
    } else {
      /* Not a valid stream */
      store_bool(nres, i, 0);
    }
  }

  return nres;
}
 

/**
 * Determine which of a list of I/O streams are readable, writeable or
 * have exceptions and return boolean arrays where true indicates readable.
 */
void inio_poll(void) {
  nialptr x = apop();
  nialptr rd_set, wr_set, ex_set, ntval, nres;
  int rd_cnt, wr_cnt, ex_cnt;
  nialint tval, res_cnt = 3;
  fd_set read_fds, write_fds, except_fds;
  fd_set *rp = &read_fds, *wp = &write_fds, *ep = &except_fds;
  struct timeval timeout;
  int res, selcnt = -1, valid_args = 1;

  /*
   * Check the type and number of arguments.
   */
  if (kind(x) != atype || tally(x) != 4) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  /* Check each argument type */
  ntval = fetch_array(x, 0);
  rd_set = fetch_array(x, 1);
  wr_set = fetch_array(x, 2);
  ex_set = fetch_array(x, 3);

  /* Validate types of arguments */
  valid_args &= (kind(ntval) == inttype);
  valid_args &= (kind(rd_set) == inttype || rd_set == Null);
  valid_args &= (kind(wr_set) == inttype || wr_set == Null);
  valid_args &= (kind(ex_set) == inttype || ex_set == Null);
  if (!valid_args) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }

  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  FD_ZERO(&except_fds);

  /* copy the time value */
  tval = intval(ntval);

  /* Build the read set */
  if (rd_set == Null) {
    rp = NULL;
  } else {
    rd_cnt = make_poll_fdset(rp, rd_set);
    selcnt = (rd_cnt > selcnt)? rd_cnt: selcnt;
  }

  /* Build the write set */
  if (wr_set == Null) {
    wp = NULL;
  } else {
    wr_cnt = make_poll_fdset(wp, wr_set);
    selcnt = (wr_cnt > selcnt)? wr_cnt: selcnt;
  }

  /* Build the except set */
  if (ex_set == Null) {
    ep = NULL;
  } else {
    ex_cnt = make_poll_fdset(ep, ex_set);
    selcnt = (ex_cnt > selcnt)? ex_cnt: selcnt;
  }


  /* Set up for the select call */
  if (selcnt != -1) {
    if (tval >= 0) {
      /* set timeout to avoid permanent blocking */
      timeout.tv_sec = tval / 1000000;
      timeout.tv_usec = tval % 1000000;
      res = select(selcnt+1, rp, wp, ep, &timeout);
    } else {
      /* select with permanent blocking */
      res = select(selcnt+1, rp, wp, ep, NULL);
    }
    
    /* Check the return code */
    if (res == -1) {
      if (errno != EINTR) {
        apush(makefault("?syserr"));
      } else {
        apush(Null);
      }
      freeup(x);
      return;
    }
  }

  /* Get here if select didn't fail */
  nres = new_create_array(atype, 1, 0, &res_cnt);
  store_array(nres, 0, make_poll_result(&read_fds, rd_set)); 
  store_array(nres, 1, make_poll_result(&write_fds, wr_set)); 
  store_array(nres, 2, make_poll_result(&except_fds, ex_set)); 

  apush(nres);
  freeup(x);
  return;
}


void inio_newpipe(void) {
  nialptr x = apop();
  int pipe1[2];
  nialptr res;
  nialint flags, *iptr, res_len = 2;
  
  if (kind(x) != inttype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  flags = intval(x);

  if (pipe(pipe1) < 0) {
    apush(makefault("?syserr"));
    freeup(x);
    return;
  }

  nio_set_nonblock(pipe1[0], flags);
  nio_set_nonblock(pipe1[1], flags);

  res = new_create_array(inttype, 1, 0, &res_len);
  iptr = pfirstint(res);
  *iptr++ = pipe1[0];
  *iptr++ = pipe1[1];

  apush(res);
  freeup(x);
  return;
}


void inio_socketpair(void) {
  nialptr x = apop();
  int sv[2];
  nialptr res;
  nialint *iptr, res_len = 2, flags;
  
  if (kind(x) != inttype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  flags = intval(x);

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
    apush(makefault("?syserr"));
    freeup(x);
    return;
  }

  nio_set_nonblock(sv[0], flags);
  nio_set_nonblock(sv[1], flags);

  res = new_create_array(inttype, 1, 0, &res_len);
  iptr = pfirstint(res);
  *iptr++ = sv[0];
  *iptr++ = sv[1];

  apush(res);
  freeup(x);
  return;
}


/**
 * res := nio_is_readable stream wait_time
 *
 * Check if we can read from a descriptor. The wait_time 
 * value (in microseconds) determines how long we will wait for
 * a result (-1 is indefinite).
 *
 * The return value is 1 if the descriptor is readable, -1 if
 * end-of-file, otherwise 0
 */
void inio_is_readable(void) {
  nialptr x = apop();
  nialint *iptr, ios, tval;
  SP_StreamPtr sp;
  int poll;

  if (kind(x) != inttype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  iptr = pfirstint(x);
  ios = *iptr++;
  tval = *iptr;

  if (!VALID_STREAM(ios)) {
    apush(makefault("?invalid_stream"));
    freeup(x);
    return;
  }
    
  sp = nio_streams[ios];

  /* Handle internal stream immediately */
  if (sp->fd == -1) {
    apush((sp->count > 0)? createint(1): createint(0));
    freeup(x);
    return;
  }

  poll = isReadable(sp->fd, tval);
  if (poll < 0) {
    sp->status = IOS_EOF;
    apush(createint(-1));
  } else if (poll == 0) {
    apush(createint(0));
  } else {
    apush(createint(1));
  }

  freeup(x);
  return;
}


/**
 * res := nio_is_writeable stream wait_time
 *
 * Check if we can write to a descriptor. The wait_time value
 * is in microseconds with -1 being indefinite
 *
 * The return value is 1 if the descriptor is readable, -1 if
 * end-of-file, otherwise 0
 */
void inio_is_writeable(void) {
  nialptr x = apop();
  nialint *iptr, ios, tval;
  SP_StreamPtr sp;
  int poll;

  if (kind(x) != inttype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  iptr = pfirstint(x);
  ios = *iptr++;
  tval = *iptr;

  
  if (!VALID_STREAM(ios)) {
    apush(makefault("?invalid_stream"));
    freeup(x);
    return;
  }

  sp = nio_streams[ios];

  /* Handle internal stream immediately */
  if (sp->fd == -1) {
    apush(createint(1));
    freeup(x);
    return;
  }
    
  poll = isWriteable(sp->fd, tval);
  if (poll < 0) {
    sp->status = IOS_EOF;
    apush(createint(-1));
  } else if (poll == 0) {
    apush(createint(0));
  } else {
    apush(createint(1));
  }

  freeup(x);
  return;
}



#endif /* SPROCESS */
