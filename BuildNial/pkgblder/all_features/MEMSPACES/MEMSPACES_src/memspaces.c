/*==============================================================

  MODULE   MEMSPACES.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  Memory spaces for Nial. 

================================================================*/


/* Q'Nial file that selects features */

#include "switches.h"

#ifdef MEMSPACES

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

/* Q'Nial header files */

#include "absmach.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"
#include "unixif.h"


#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>

#include "mcore.h"
#include "memspaces.h"


#define MAX_MEM_SPACES   128
static int num_spaces = 0;
static MemSpace mem_spaces[MAX_MEM_SPACES];


/**
 * Allocate a memory space and return the index.
 * Return -1 if no spaces.
 */
static int alloc_space() {
  int i;

  for (i = 0; i < num_spaces; i++) {
    if (mem_spaces[i].handle == NULL)
      return i;
  }

  if (i < MAX_MEM_SPACES) {
    num_spaces++;
    return i;
  }

  return -1;
}
  

/* -------------------- Low Level Code -------------------------- */

/**
 * Compute the number of bytes for a given type
 */ 
static nialint msp_nbytes(nialint mt, nialint ms) {
  nialint res;

  switch(mt) {
  case MSP_BOOLTYPE:
    res = boolsPW*((ms+boolsPW - 1)/boolsPW);
    break;

  case MSP_INTTYPE:
    res = ms*sizeof(nialint);
    break;

  case MSP_REALTYPE:
    res = ms*sizeof(double);
    break;

  case MSP_CHARTYPE:
    res = charsPW*((ms+charsPW - 1)/charsPW);
    break;

  default:
    res = -1;
    break;
  }    

  return res;
}


/**
 * Estimate the size of am memory segment
 * The parameters are -
 *
 * 1.  The type of the segment, one of MSP_INTTYPE, MSP_BOOLTYPE,
 *     MSP_REALTYPE or MSP_CHARTYPE
 * 2.  The number of entries of that type
 *
 * The return value is the byte count
 */
void imsp_msize(void) {
  nialptr x = apop();
  nialint mt, ms, *iptr;

  if (kind(x) != inttype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  apush(createint(msp_nbytes(fetch_int(x, 0), fetch_int(x, 1))));
  freeup(x);
  return;
}


/**
 * Copy data from a memory space into the Nial workspace.
 * The parameters are -
 *
 * 1.  The memory space index
 * 2.  The type of data to create (nial type)
 * 3.  The byte offset from the start of the memory space
 * 4.  The number of entries to copy (type dependent)
 *
 * The return value is a nial object.
 */
void imsp_get_raw(void) {
  nialptr x = apop();
  nialint *iptr, mi, mt, mbo, ment, mbytes;
  nialptr res;

  if (kind(x) != inttype || tally(x) != 4) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  iptr = pfirstint(x);
  mi = *iptr++;
  mt = *iptr++;
  mbo = *iptr++;
  ment = *iptr++;
  mbytes = msp_nbytes(mt, ment);

  if (mbytes <= 0) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  res = new_create_array(mt, 1, 0, &ment);
  memcpy(pfirstchar(res), (unsigned char*)mem_spaces[mi].mbase+mbo, mbytes);

  apush(res);
  freeup(x);
  return;
}


/*
 * Copy data to a memory space from the Nial workspace.
 * The parameters are -
 *
 * 1.  The memory space index
 * 2.  The nial data object
 * 3.  The byte offset from the start of the memory space
 * 4.  The number of entries to copy (type dependent)
 *
 */
void imsp_put_raw(void) {
  nialptr x = apop();
  nialptr nms, nob, nbo, nent;
  nialint msi, nbytes;

  if (kind(x) != atype || tally(x) != 4) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  nms = fetch_array(x, 0);
  nob = fetch_array(x, 1);
  nbo = fetch_array(x, 2);
  nent = fetch_array(x,3);

  if (kind(nms)!= inttype || kind(nbo) != inttype || kind(nent) != inttype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  msi = intval(nms);
  nbytes = msp_nbytes(kind(nob), intval(nent));
  if (nbytes <= 0) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  memcpy((unsigned char*)mem_spaces[msi].mbase+intval(nbo), pfirstchar(nob), nbytes);

  apush(True_val);
  freeup(x);
  return;
}


/**
 * Create a POSIX named shared memory space. This routine returns
 * a file descriptor which can then be used to mmap the space. 
 *
 * The routine takes three parameters
 *
 * 1.  The name of the space 
 * 2.  The code to open the space
 * 3.  Permission values
 *
 * It returns a file descriptor on success and -1 on failure.
 *
 */
void imsp_shm_open(void) {
  nialptr x = apop();
  nialptr nname, nmode, nopen; 
  nialint mopen, mmode;
  int fd, flags = 0;

  if (kind(x) != atype || tally(x) != 3) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  /* Get the individual values */
  nname  = fetch_array(x, 0);
  nopen  = fetch_array(x, 1);
  nmode  = fetch_array(x, 2);

  /* validate types */
  if (kind(nname) != chartype || kind(nopen) != inttype || kind(nmode) != inttype) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }


  /* Set up the open flags */
  mopen  = intval(nopen);
  flags = (mopen&1)? O_RDONLY: O_RDWR;
  if (mopen & 2) flags |= O_CREAT;
  if (mopen & 4) flags |= O_EXCL;
 
  /* Set up the mode */
  mmode = intval(nmode);
  if (mmode == 0) mmode = 0777;
  
  /* try to create */
  fd = shm_open(pfirstchar(nname), flags, mmode);
  if (fd == -1)
  { apush(makefault(strerror(errno)));}
  else
    apush(createint(fd));

  freeup(x);
  return;
}


/**
 * Unlink a shared memory space. The routine takes the
 * name of the shared memory space as its argument.
 */
void imsp_shm_unlink(void) {
  nialptr x = apop();
  
  if (kind(x) != chartype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  apush(createint(shm_unlink(pfirstchar(x))));
  freeup(x);
  return;
}

  
/**
 * Create a memory space that can be shared amongst the
 * children of this process. This takes two parameters
 *
 * 1.  The size of the space to create.
 * 2.  Flags to shape the access model
 *
 */
void imsp_map_local(void) {
  nialptr x = apop();
  nialint *iptr;
  nialint msize, mflags;
  int mi, flags;
  void *handle;

  if (kind(x) != inttype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  mi = alloc_space();
  if (mi == -1) {
    apush(makefault("?nomem"));
    freeup(x);
    return;
  }

  iptr = pfirstint(x);
  msize = *iptr++;
  mflags = *iptr++;
  flags = (mflags)?(MAP_PRIVATE|MAP_ANONYMOUS):(MAP_SHARED|MAP_ANONYMOUS);

  handle = mmap(NULL, msize, PROT_READ|PROT_WRITE, flags, -1, 0);
  if (handle == MAP_FAILED) {
    apush(makefault("?nomem"));
    freeup(x);
    return;
  }

  mem_spaces[mi].mtype   = MSP_LOCAL;
  mem_spaces[mi].handle  = handle;
  mem_spaces[mi].mbase   = handle;
  mem_spaces[mi].msize   = msize;
  

  apush(createint(mi));
  freeup(x);
  return;
}


/**
 * Create a memory mapped file. This takes two parameters
 *
 * 1.  The size of the space to create.
 * 2.  Flags to shape the access model
 * 3.  The fd of the file to map 
 */
void imsp_map_fd(void) {
  nialptr x = apop();
  nialint *iptr;
  nialint msize, mflags, mfd;
  int mi, flags, fd, prot = PROT_READ;
  void *handle;
  struct stat buf;

  if (kind(x) != inttype || tally(x) != 3) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  mi = alloc_space();
  if (mi == -1) {
    apush(makefault("?nomem"));
    freeup(x);
    return;
  }

  iptr = pfirstint(x);
  msize = *iptr++;
  mflags = *iptr++;
  mfd = *iptr++;

  flags = (mflags&01)?MAP_PRIVATE:MAP_SHARED;
  prot  = (mflags&02)?PROT_READ: PROT_READ|PROT_WRITE;

  /* If specified size is zero then use whole file */
  if (msize == 0) {
    if (fstat((int)mfd, &buf) == -1) {
      apush(makefault("?no_file"));
      freeup(x);
      return;
    }
    msize = buf.st_size;
  }
  
  /* Map the file */
  handle = mmap(NULL, msize, prot, flags, mfd, 0);
  if (handle == MAP_FAILED) {
    apush(makefault("?nomem"));
    freeup(x);
    return;
  }

  mem_spaces[mi].mtype   = MSP_SHARED;
  mem_spaces[mi].handle  = handle;
  mem_spaces[mi].mbase   = handle;
  mem_spaces[mi].msize   = msize;
  
  apush(createint(mi));
  freeup(x);
  return;
}


/* -------------------- Synchronisation -------------------- */


/**
 * Compare and swap
 *
 * Args, all ints  -
 *
 * 1.  memory space index
 * 2.  memory offset
 * 3.  compare value
 * 4.  replacement value
 */
void imsp_cas(void) {
  nialptr x = apop();
  nialint msi, offs, casval, repval;
  nialint *iptr;
  int res;

  if (kind(x) != inttype || tally(x) != 4) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  iptr   = pfirstint(x);
  msi    = *iptr++;
  offs   = *iptr++;
  casval = *iptr++;
  repval = *iptr++;

  if ((offs % sizeof(nialint)) != 0) {
    apush(makefault("?alignment"));
    freeup(x);
    return;
  }

  res = AtomicCompareAndSwap((nialint*)(mem_spaces[msi].mbase+offs), casval, repval);

  apush((res)?True_val:False_val);
  freeup(x);
  return;
}


/* ------------------- System Config Values ---------------- */


#define NSYSC_CONF_CORES         0
#define NSYSC_AVAIL_CORES        1


/**
 * Return system configuration parameters 
 */
void imsp_sysconfig(void) {
  nialptr x = apop();
  nialint res;

  if (kind(x) != inttype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  switch (intval(x)) {
  case NSYSC_CONF_CORES:
    res = sysconf(_SC_NPROCESSORS_CONF);
    break;

  case NSYSC_AVAIL_CORES:
    res = sysconf(_SC_NPROCESSORS_ONLN);
    break;

  default:
    res = -1;
    break;
  }

  apush(createint(res));
  freeup(x);
  return;
}

#endif /* MEMSPACES */


