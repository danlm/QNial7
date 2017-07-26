/**
 * ndynload.c 
 * --------
 *
 * Dynamic loading of shared libraries for Nial.
 Contrubuted by John Gibbons.
 *
 * For Linux and Mac OSX this wraps the dlfnc capabilities of the OS.
 * 
 */


#include "switches.h"


#ifdef NDYNLOAD

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

/* SJLIB */
#include <setjmp.h>

/* MATHLIB */
#include <math.h>

/* Q'Nial header files */

#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "ops.h"

#include <dlfcn.h>
#include <string.h>


typedef void (*NialPrimFunction)(void);


/* Flags to identify type of struct */ 
#define NDL_LIBRARY_CAST 1
#define NDL_FUNCTION_CAST 2


/* Routine to move between structs and arrays of integers */
static nialptr ndl_copy_struct(void *p, nialint nbytes);
static nialint ndl_restore_struct(void *p, nialint nbytes, nialptr inta);


/* Macro to simplify copy of a struct to an array of nial ints */
#define NDL_COPY_STRUCT(strct, ptr) ndl_copy_struct(ptr, sizeof(strct))

/* Macro to simplify restore of a struct from an array of integers */
#define NDL_RESTORE_STRUCT(strct, ptr, inta) ndl_restore_struct(ptr, sizeof(strct), inta)

 
 /* Common struct for libraries and routines */
typedef struct { 
  nialint castType;
  void *ptrVal;
} PointerCast;


/**
 * Copy a struct to an array of integers
 */
static nialptr ndl_copy_struct(void* p, nialint nbytes) {
  nialint *ip;
  nialptr np;
  nialint shp;
  
  shp = sizeof(nialint)*((nbytes + sizeof(nialint) - 1)/sizeof(nialint));
  
  np = new_create_array(inttype, 1, 0, &shp);
  ip = pfirstint(np);
  memcpy(ip, p, nbytes);
  
  return np;
}


/**
 * Restore a struct from an array of integers
 */
static nialint ndl_restore_struct(void* p, nialint nbytes, nialptr inta) {

  if (kind(inta) != inttype || valence(inta) != 1 ||  nbytes > tally(inta)*sizeof(nialint))
  	return -1;
  else {
    memcpy(p, pfirstint(inta), nbytes);
    return 0;
  }
}


/**
 * Returns the last error code or NULL if none
 */
void indlError(void) {
  nialptr x = apop();
  char *err = dlerror();    /* Get last error */

  if (err == NULL) {
    apush(Null);
  } else {
    apush(makestring(err));
  }

  freeup(x);
  return;
}


/**
 * Load a dynamic library into the Nial address space.
 */
void indlLoad(void) {
  PointerCast p;
  nialptr x = apop();
  
  /* verify atom or char array supplied as name */
  if (istext(x)) {
    char *libname = pfirstchar(x);
    void *dlp = dlopen(libname, RTLD_NOW|RTLD_LOCAL);
    
    /* Check the open succeeded */
    if(dlp == NULL) {
    	apush(makefault("?ndlLoad failed"));
    } else {
      p.castType = NDL_LIBRARY_CAST;
      p.ptrVal = dlp;
      apush(NDL_COPY_STRUCT(PointerCast, &p));
    } 
  } else {
    apush(makefault("?ndlLoad requires library name"));
  }
  
  freeup(x);
  return;
}


/**
 * Find the function primitive corresponding to a supplied name
 */
void indlGetsym(void) {
  nialptr x = apop();
  PointerCast libcast, symcast;
  
  if (tally(x) == 2) {
    nialptr nlib, nsymn;
    
    /* Get library and symbol and validate */
    splitfb(x, &nlib, &nsymn);
    if (istext(nsymn) && NDL_RESTORE_STRUCT(PointerCast, &libcast, nlib) != -1 && libcast.castType == NDL_LIBRARY_CAST) {
      void* psym = dlsym( libcast.ptrVal, pfirstchar(nsymn));
      
      if(psym == NULL) {
	    apush(makefault("?ndlGetsym failed"));
      } else {
        symcast.castType = NDL_FUNCTION_CAST;
        symcast.ptrVal = psym;
	    apush(NDL_COPY_STRUCT(PointerCast, &symcast));
      }
    } else {
      apush(makefault("?ndlGetsym bad arguments"));
    }
  } else {
    apush(makefault("?ndlGetsym expects two arguments"));
  }
  
  freeup(x);
  return;
}


/**
 * Close a shared library
 */
void indlClose(void) {
  nialptr nlib = apop();
  PointerCast libcast;

  if (NDL_RESTORE_STRUCT(PointerCast, &libcast, nlib) != -1 && libcast.castType == NDL_LIBRARY_CAST) {
    dlclose(libcast.ptrVal);                          /* Close the handle       */ 
    memset(pfirstint(nlib), 0, sizeof(PointerCast));   /* Clear cast and pointer */
    apush(True_val);
  } else {
    apush(makefault("?dynlib"));
  }
  
  freeup(nlib);
}


/** 
 * Call a function from a shared library
 */
void indlCall(void) {
  nialptr x = apop();
  nialptr fun;
  nialptr funargs;
  PointerCast funcast;

  if (tally(x) == 2) {
    splitfb(x, &fun, &funargs);

    if (NDL_RESTORE_STRUCT(PointerCast, &funcast, fun) != -1 && funcast.castType == NDL_FUNCTION_CAST) {
      /* invoke the externalfunction */
      NialPrimFunction nfptr = (NialPrimFunction)(funcast.ptrVal);
      apush(funargs);
      (nfptr)();
    } else {
      apush(makefault("?dynfun"));
    }

  } else {
    apush(makefault("?args"));
  }

  freeup(x);
  return;
}


#endif /* NDYNLOAD */
