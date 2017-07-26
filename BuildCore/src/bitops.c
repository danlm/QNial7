 /**
 * bitops.c
 *
 * Contributed by John Gibbons
 * 
 * --------
 *
 * This module provides a series of low level bit manipulation operations
 * that are designed purely for performance considerations.
 *
 * They operate on character arrays, integer arrays, float arrays
 * and boolean arrays.
 *
 * There are two core static routines that perform all the operations
 * based on a supplied operation code, 'bitops1' for conformant arrays
 * and 'bitops2' for one atomic argument. Everything else is treated as
 * an error.
 * 
 *  int bitops1(nialint *res, nialint *arg1, nialint *arg2, nialint alen, nialint carry, int opcode)
 *  int bitops2(nialint *res, nialint *arg1, nialint *arg2, nialint alen, nialint carry, int opcode)
 *  
 */
 

#include "switches.h"


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

/* SJLIB */
#include <setjmp.h>

/* MATHLIB */
#include <math.h>

#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "ops.h"
#include "fileio.h"

#include <string.h>

/* The handled operations */
#define NBIT_AND      1
#define NBIT_OR       2
#define NBIT_XOR      3
#define NBIT_NOT      4


/**
 *  Perform the required bit operation with integer data
 */
static void bitops_int(nialptr res, nialptr arg1, nialptr arg2, int opcode) {
  nialint t1 = tally(arg1), t2 = tally(arg2);
  nialint *ipres = pfirstint(res), *ip1, *ip2;
  nialint mask, count;
  nialint wc;
  
  if (t1 != t2) {
    /* Different tallies and one is 1 */
    if (t1 == 1) {
      mask = fetch_int(arg1, 0);
      ip1 = pfirstint(arg2);
      count = t2;
    } else {
      mask = fetch_int(arg2, 0);
      ip1 = pfirstint(arg1);
      count = t1;
    }
    
    switch(opcode) {
      case NBIT_AND:
        for (wc = 0; wc < count; wc++)
          *ipres++ = mask & *ip1++;
        break;
      case NBIT_OR:
        for (wc = 0; wc < count; wc++)
          *ipres++ = mask | *ip1++;
        break;
      case NBIT_XOR:
        for (wc = 0; wc < count; wc++)
          *ipres++ = mask ^ *ip1++;
        break;
    }
    
  } else {
    /* operation with conformant arrays */
    ip1 = pfirstint(arg1);
    ip2 = pfirstint(arg2);
    switch (opcode) {
      case NBIT_AND:
        for (wc = 0; wc < t1; wc++)
          *ipres++ = *ip1++ & *ip2++;
        break;
      
      case NBIT_OR:
        for (wc = 0; wc < t1; wc++)
          *ipres++ = *ip1++ | *ip2++;
        break;
      
      case NBIT_XOR:
        for (wc = 0; wc < t1; wc++)
          *ipres++ = *ip1++ ^ *ip2++;
        break;
    }
  }
  
}

static void bitops_char(nialptr res, nialptr arg1, nialptr arg2, int opcode) {
  nialint t1 = tally(arg1), t2 = tally(arg2);
  unsigned char *ipres = (unsigned char *)pfirstchar(res), *ip1, *ip2;
  unsigned char mask;
  nialint count;
  nialint wc;
  
  if (t1 != t2) {
    /* Different tallies and one is 1 */
    if (t1 == 1) {
      mask = (unsigned char)(0xFF & fetch_char(arg1, 0));
      ip1 = (unsigned char *)pfirstchar(arg2);
      count = t2;
    } else {
      mask = (unsigned char)(0xFF & fetch_char(arg2, 0));
      ip1 = (unsigned char *)pfirstchar(arg1);
      count = t1;
    }
    
    switch(opcode) {
      case NBIT_AND:
        for (wc = 0; wc < count; wc++)
          *ipres++ = mask & *ip1++;
        break;
      case NBIT_OR:
        for (wc = 0; wc < count; wc++)
          *ipres++ = mask | *ip1++;
        break;
      case NBIT_XOR:
        for (wc = 0; wc < count; wc++)
          *ipres++ = mask ^ *ip1++;
        break;
    }
    
  } else {
    /* operation with conformant arrays */
    ip1 = (unsigned char *)pfirstchar(arg1);
    ip2 = (unsigned char *)pfirstchar(arg2);
    switch (opcode) {
      case NBIT_AND:
        for (wc = 0; wc < t1; wc++)
          *ipres++ = *ip1++ & *ip2++;
        break;
      
      case NBIT_OR:
        for (wc = 0; wc < t1; wc++)
          *ipres++ = *ip1++ | *ip2++;
        break;
      
      case NBIT_XOR:
        for (wc = 0; wc < t1; wc++)
          *ipres++ = *ip1++ ^ *ip2++;
        break;
    }
  }
}

static void bitops_bool(nialptr res, nialptr arg1, nialptr arg2, int opcode) {
  nialint t1 = tally(arg1), t2 = tally(arg2);
  nialint *ipres = pfirstint(res), *ip1, *ip2;
  nialint mask, count;
  nialint wc;
  
  if (t1 != t2) {
    /* Different tallies and one is 1 */
    if (t1 == 1) {
#ifdef INTS64
      mask = (0 == fetch_bool(arg1, 0))? 0: 0xFFFFFFFFFFFFFFFF;
#endif
#ifdef INTS32
      mask = (0 == fetch_int(arg1, 0))? 0: 0xFFFFFFFF;
#endif
      ip1 = pfirstint(arg2);
      count = (t2 + boolsPW - 1)/boolsPW;
    } else {
#ifdef INTS64
      mask = fetch_int(arg2, 0);
#endif
#ifdef INTS32
      mask = fetch_int(arg2, 0);
#endif
      ip1 = pfirstint(arg1);
      count = (t1 + boolsPW - 1)/boolsPW;
    }
    
    switch(opcode) {
      case NBIT_AND:
        for (wc = 0; wc < count; wc++)
          *ipres++ = mask & *ip1++;
        break;
      case NBIT_OR:
        for (wc = 0; wc < count; wc++)
          *ipres++ = mask | *ip1++;
        break;
      case NBIT_XOR:
        for (wc = 0; wc < count; wc++)
          *ipres++ = mask ^ *ip1++;
        break;
    }
    
  } else {
    /* operation with conformant arrays */
    ip1 = pfirstint(arg1);
    ip2 = pfirstint(arg2);
    count = (t1 + boolsPW - 1)/boolsPW;
    switch (opcode) {
      case NBIT_AND:
        for (wc = 0; wc < count; wc++)
          *ipres++ = *ip1++ & *ip2++;
        break;
      
      case NBIT_OR:
        for (wc = 0; wc < count; wc++)
          *ipres++ = *ip1++ | *ip2++;
        break;
      
      case NBIT_XOR:
        for (wc = 0; wc < count; wc++)
          *ipres++ = *ip1++ ^ *ip2++;
        break;
    }
  }
}


/**
 * Allocate a result vector of the appropriate type, valence and shape
 * and redirect code to the appropriate routine to do the work.
 */
static nialptr bitops_all(nialptr arg1, nialptr arg2, int opcode) {
  nialint t1 = tally(arg1), t2 = tally(arg2);
  nialptr res;
  nialint resv, *shp;
  
  /* Work out the valence and shape of the result */
  if (t1 == 1) {
    resv = valence(arg2);
    shp  = shpptr(arg2, resv);
  } else {
    resv = valence(arg1);
    shp  = shpptr(arg1, resv);
  }
  
  /* allocate the output */
  apush(arg1);              /* hold for safety */
  apush(arg2);              /* hold for safety */
  res = new_create_array(kind(arg1), resv, 0, shp);
  apop();
  apop();
  
  /*
   * The data portion of all arrays is measured in words,
   * work out the word count for the bit operations based on types
   */
  switch (kind(arg1)) {
    case inttype:
      bitops_int(res, arg1, arg2, opcode);
      break;
    
    case chartype:
      bitops_char(res, arg1, arg2, opcode);
      break;
    
    case booltype:
      bitops_bool(res, arg1, arg2, opcode);
      break;
  }

  return res;
}
   

void ibit_and() {
  nialptr x = apop();
  nialptr a1, a2;
  nialint *ip;
  char *cp;
  nialint bv1, bv2;
  nialptr res;
  
  /* Two arguments are required */
  if (tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  /* Check the type of the arguments */
  switch(kind(x)) {
    case atype:
      /* Check the arguments for type and conformance */
      a1 = fetch_array(x, 0);
      a2 = fetch_array(x, 1);
      /* First check for conformance */
      if (kind(a1) != kind(a2) || (tally(a1 > 1) && tally(a2) > 1 && tally(a1) != tally(a2))) {
        apush(makefault("?not_conformant"));
        freeup(x);
        return;
      }
      /* Then check for valid type */
      switch (kind(a1)) {
        case inttype:
        case chartype:
        case booltype:
          res = bitops_all(a1, a2, NBIT_AND);
          break;
        
        default:
          apush(makefault("?unsupported_type"));
          freeup(x);
          return;
      }
      break;
    
    case inttype:
      {
        /* Two simple integers */
        register nialint *ip = pfirstint(x);
        register nialint ni1 = *ip++;
        register nialint ni2 = *ip++;
        res = createint(ni1 & ni2);
      }
      break;
    
    case booltype:
      /* two simple booleans */
      res = createbool(fetch_bool(x,0) & fetch_bool(x,1));
      break;
    
    case chartype:
      {
        /* Two simple chars */
        register unsigned char *cp = (unsigned char *)pfirstchar(x);
        register unsigned char ch1 = *cp++, ch2 = *cp++;
        res = createchar((ch1 & ch2)&0xFF);
      }
      break;
    
    default:
      apush(makefault("?unsupported_type"));
      freeup(x);
      return;
  }
  
  apush(res);
  freeup(x);
  return;
}


void ibit_or() {
  nialptr x = apop();
  nialptr a1, a2;
  nialint *ip;
  char *cp;
  nialptr res;

  /* Two arguments are required */
  if (tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  /* Check the type of the arguments */
  switch(kind(x)) {
    case atype:
      /* Check the arguments for type and conformance */
      a1 = fetch_array(x, 0);
      a2 = fetch_array(x, 1);
      /* First check for conformance */
      if (kind(a1) != kind(a2) || (tally(a1 > 1) && tally(a2) > 1 && tally(a1) != tally(a2))) {
        apush(makefault("?not_conformant"));
        freeup(x);
        return;
      }
      /* Then check for valid type */
      switch (kind(a1)) {
        case inttype:
        case chartype:
        case booltype:
          res = bitops_all(a1, a2, NBIT_OR);
          break;
               
        default:
          apush(makefault("?unsupported_type"));
          freeup(x);
          return;
      }
      break;
    
    case inttype:
      {
        /* Two simple integers */
        register nialint *ip = pfirstint(x);
        register nialint ni1 = *ip++;
        register nialint ni2 = *ip++;
        res = createint(ni1 | ni2);
      }
      break;
    
    case booltype:
      res = createbool(fetch_bool(x,0) | fetch_bool(x,1));
      break;
    
    case chartype:
      {
        /* Two simple chars */
        register unsigned char *cp = (unsigned char *)pfirstchar(x);
        register unsigned char ch1 = *cp++, ch2 = *cp++;
        res = createchar((ch1 | ch2)&0xFF);
      }
      break;
    
    default:
      apush(makefault("?unsupported_type"));
      freeup(x);
      return;
  }
    
  apush(res);
  freeup(x);
  return;
}


void ibit_xor() {
  nialptr x = apop();
  nialptr a1, a2;
  nialint *ip;
  char *cp;
  nialptr res;
  
  /* Two arguments are required */
  if (tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  /* Check the type of the arguments */
  switch(kind(x)) {
    case atype:
      /* Check the arguments for type and conformance */
      a1 = fetch_array(x, 0);
      a2 = fetch_array(x, 1);
      /* First check for conformance */
      if (kind(a1) != kind(a2) || (tally(a1 > 1) && tally(a2) > 1 && tally(a1) != tally(a2))) {
        apush(makefault("?not_conformant"));
        freeup(x);
        return;
      }
      /* Then check for valid type */
      switch (kind(a1)) {
        case inttype:
        case booltype:
        case chartype:
          res = bitops_all(a1, a2, NBIT_XOR);
          break;
        
        default:
          apush(makefault("?unsupported_type"));
          freeup(x);
          return;
      }
      break;
    
    case inttype:
      {
        /* Two simple integers */
        register nialint *ip = pfirstint(x);
        register nialint ni1 = *ip++;
        register nialint ni2 = *ip++;
        res = createint(ni1 ^ ni2);
      }
      break;
    
    case booltype:
      res = createbool(fetch_bool(x,0) ^ fetch_bool(x,1));
      break;
    
    case chartype:
      {
        /* Two simple chars */
        register unsigned char *cp = (unsigned char *)pfirstchar(x);
        register unsigned char ch1 = *cp++, ch2 = *cp++;
        res = createchar((ch1 ^ ch2)&0xFF);
      }
      break;
    
    default:
      apush(makefault("?unsupported_type"));
      freeup(x);
      return;
  }
  
  apush(res);
  freeup(x);
  return;
}

#ifdef JG_NOT_YET
void ibit_not() {
  nialptr x = apop();
  nialptr res;

  /* Two arguments are required */
  if (tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  /* Check the type of the arguments */
  switch(kind(x)) {
    case atype:
      break;
    
    case inttype:
      ip = pfirstint(x);
      res := createint(*ip++ & *ip++);
      break;
    case booltype:
      res = createbool(fetch_bool(x,0) & fetch_bool(x,1));
      break;
    
    case chartype:
      cp = pfirstchar(x);
      res = createchar((*cp++ & *cp++)&0xFF);
      break;
    
    default:
      apush(makefault("?unsupported_type"));
      freeup(x);
      return;
  }
    
  apush(res);
  freeup(x);
  return;
}

#endif


/**
 * Returns the content of an array as a char array. This is only
 * valid for arrays of basic types. 
 */
void iraw_bytes(void) {
  nialptr x = apop();
  nialptr res;
  nialint reslen, t;
  
  t = tally(x);
  if (t == 0) {
    apush(Null);
    freeup(x);
    return;
  }
  
  /* Check type and compute the byte count for the result */
  switch (kind(x)) {
    case inttype:
      reslen = t*sizeof(nialint);
      break;
    
    case realtype:
      reslen = t*sizeof(double);
      break;
    
    case chartype:
      reslen = t;
      break;
      
    case booltype:
      reslen = (t + CHAR_BIT - 1)/CHAR_BIT;
      break;
    
    default:
      apush(makefault("?args"));
      freeup(x);
      return;
  }
  
  /* Allocate the result and copy */
  res = new_create_array(chartype, 1, 0, &reslen);
  memcpy((void*)pfirstchar(res), (void*)pfirstchar(x), reslen);
  
  /* Return */
  apush(res);
  freeup(x);
  return;
}


/**
 * Return the character code of the argument as an unsigned
 * integer
 */
void ibyterep(void) {
  nialptr x = apop();
  nialint i, t = tally(x);
  nialint *shptr, v;
  unsigned char *cp;
  nialint *ip;
  nialptr res;
  
  if (kind(x) != chartype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  /* Create the result array */
  v = valence(x);
  shptr = shpptr(x, v);
  res = new_create_array(inttype, v, 0, shptr);
  
  /* Convert */
  ip = pfirstint(res);
  cp = (unsigned char *)pfirstchar(x);
  for (i = 0; i < t; i++)
    *ip++ = (nialint)*cp++;
  
  apush(res);
  freeup(x);
  return;
}


