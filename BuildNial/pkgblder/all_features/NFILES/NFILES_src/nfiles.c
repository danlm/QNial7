/*==============================================================

  MODULE   NFILES.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This contains the system independent directory operations
   and support routines

================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

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

/* STDLIB */
#include <stdlib.h>
#ifdef UNIXSYS
#include <limits.h>
#endif

/* STLIB */
#include <string.h>

/* SJLIB */
#include <setjmp.h>

/* CLIB */
#include <ctype.h>

/* VARARGSLIB */
#include <stdarg.h>
#include <stddef.h>


/* Q'Nial header files */

#include "qniallim.h"
#include "absmach.h"
#include "basics.h"
#include "lib_main.h"
#include "if.h"
#include "ops.h"

#include "utils.h"           /* for ngetname */

#ifdef NFILES

/**
 * Create a new directory. This takes two parameters, name and mode.
 * On Windows the mode parameter is ignored. If it is not present on
 * Unix it defaults to 0777.
 */
void imkdir(void) {
  nialptr x = apop();
  nialptr dname, dmode = invalidptr;
  int r;
  
  if (kind(x) == atype && tally(x) == 2) {
    splitfb(x, &dname, &dmode);
    if (!istext(dname) || tally(dname) == 0 || kind(dmode) != inttype) {
      apush(makefault("?invalid argument pair"));
      freeup(x);
      return;
    }
  } else if (istext(x)) {
    dname = x;
  } else {
    apush(makefault("?args"));
  }
  
#ifdef UNIXSYS
  r = mkdir(pfirstchar(dname), (dmode == invalidptr)? 0777: (mode_t)intval(dmode));
#endif
#ifdef WINNIAL
  r = mkdir(pfirstchar(dname));
#endif
  apush((r==-1)? False_val: True_val);
  freeup(x);
  return;
}


/**
 * Remove a directory
 */
void irmdir(void) {
  nialptr x = apop();
  
  if (istext(x)) {
    int res = rmdir(pfirstchar(x));
    apush((res == -1)? False_val: True_val);
  } else {
    apush(makefault("?args"));
  }
  
  freeup(x);
  return;
}


/**
 * Rename a file or directory
 */
void irename(void) {
  nialptr x = apop();
  nialptr to_ptr, from_ptr;
  
  if (kind(x) == atype || tally(x) == 2) {
    splitfb(x, &from_ptr, &to_ptr);
    if (istext(from_ptr) &&  istext(to_ptr)) {
      int res = rename(pfirstchar(from_ptr), pfirstchar(to_ptr));
      apush((res == -1)? False_val: True_val);
      freeup(x);
      return;
    }
  }
  
  apush(makefault("?args"));
  freeup(x);
  return;
}


/**
 * Return the full path name for a supplied relative path name
 */
void ifullpathname(void) {
  nialptr x = apop();
  char *res;
  
  if (istext(x) && tally(x) > 0) {
#ifdef WINNIAL
    char absPath[_MAX_PATH];
    res = _fullpath(absPath, pfirstchar(x), _MAX_PATH);
#endif
#ifdef UNIXSYS
    char rpath[PATH_MAX];
    res = realpath((const char *)pfirstchar(x), rpath);
#endif
  } else {
    apush(makefault("?args"));
  }
  
  if (res != NULL) {
    mkstring(res);
  } else {
    apush(Null);
  }
  
  freeup(x);
  return;
}

#endif  /* NFILES */

