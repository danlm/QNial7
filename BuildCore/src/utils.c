/* ==============================================================

   MODULE     UTILS.C

   COPYRIGHT NIAL Systems Limited  1983-2016

   holds utility routines shared by several modules.

================================================================*/

/* Q'Nial file that selects features */

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

/* LIMITLIB */
#include <limits.h>

/* STDLIB */
#include <stdlib.h>

/* STLIB */
#include <string.h>

/* SJLIB */
#include <setjmp.h>

/* MATHLIB */
#include <math.h>

/* CLIB */
#include <ctype.h>


/* Q'Nial header files */

#include "utils.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"

#include "scan.h"            /* for Upper */
#include "faults.h"          /* for Logical */

static nialptr to_int(nialptr x);

/* routine to convert an array of ints to reals */

nialptr
int_to_real(nialptr x)
{
  nialint     i,
             *px,
              t = tally(x);
  nialptr     z;
  double     *pz;
  int         v = valence(x);

  z = new_create_array(realtype, v, 0, shpptr(x, v));
  px = pfirstint(x);         /* safe */
  pz = pfirstreal(z);        /* safe */
  for (i = 0; i < t; i++)
    *pz++ = 1.0 * *px++;
  freeup(x);
  return (z);
}

/* routine to convert an array of bools to reals */

nialptr
bool_to_real(nialptr x)
{
  nialint     i,
              t = tally(x);
  nialptr     z;
  double     *pz;
  int         v = valence(x);

  z = new_create_array(realtype, v, 0, shpptr(x, v));
  pz = pfirstreal(z);        /* safe */
  for (i = 0; i < t; i++)
    *pz++ = 1.0 * fetch_bool(x, i);
  freeup(x);
  return (z);
}


/* finds highest atomic type in a simple array and does conversion if numeric.
   This code depends on the atomic types being ordered as they are
   with boolean the lowest and the numeric types above it in
   the order integer and real.  Since this routine is
   only called on a simple array, the value of atype does not
   affect the algorithm.
   */

nialptr
arithconvert(nialptr x, int *newk)
{
  int         k,
              ki,
              changed;
  nialint     i,
              t = tally(x);
  nialptr     z,
              x0,
              xi;

  if (t == 0) { /* x is empty */
    *newk = kind(x);
    return (x);
  }

  /* set k based on kind of first item */
  x0 = fetch_array(x, 0);    /* first item */
  k = kind(x0);

  /* loop to find maximum kind in x */
  for (i = 1; i < t; i++) {  
    xi = fetch_array(x, i);
    ki = kind(xi);
    if (ki > k)
      k = ki;
  }

  /* convert to highest kind */
  changed = true;
  if (homotype(k) && k != chartype) {
    switch (k) {
      case inttype:
          z = to_int(x);
          break;

      case realtype:
          z = to_real(x);
          break;
      default:

          z = x;
          changed = false;
          break;
    }
  }
  else {
    z = x;
    changed = false;
  }

  if (changed)               /* argument has been converted */
    freeup(x);
  *newk = k;
  return (z);
}

/* the following routine is used in the reductive pervasive
   routines to decide on what fault value to return. 
   If all the fault values are the same and there are no
   non-numeric items, then use the fault value found, otherwise
   use the stdfault given as the argument.
*/

nialptr
testfaults(nialptr x, nialptr stdfault)
{
  int         found = false;
  nialint     t = tally(x),
              i = 0;
  nialptr     xi = Null,
              z;

  /* find first fault */
  while (!found && i < t) {
    xi = fetch_array(x, i++);
    if (kind(xi) == chartype || kind(xi) == phrasetype)
      return (stdfault);     /* result is stdfault if other literal types occur */
    found = kind(xi) == faulttype;

  }

  if (!found)
    return (stdfault);       /* used in "and" and "or" in cases like "and o -12" */

  z = xi;


  /* find other faults and compare */
  { while (i < t) {
      xi = fetch_array(x, i++);
      if ((kind(xi) == faulttype && z != xi) || kind(xi) == chartype ||
          kind(xi) == phrasetype) /* result is stdfault if fault value changes
                                     or other literal type */
        return (stdfault);
    }
  }
  freeup(stdfault);   /* since this is a temporary and not used */
  return (z);
}

/* the following routine is used in the binary pervasive
   routines to decide on what fault value to return. It is known
   that one or both of the args are faults. The rule is that
   if one argument is a fault and the other is not, the fault
   value is returned. If both are faults then if they are the same
   it is returned, otherwise the stdfault is returned.
   Divide is a special case.
   */

nialptr
testbinfaults(nialptr x, nialptr stdfault, int divflag)
{
  nialptr     x0 = fetch_array(x, 0),
              x1 = fetch_array(x, 1);

  switch (kind(x0)) {
    case booltype:
    case inttype:
    case realtype:
        freeup(stdfault);
        return (x1);
    case chartype:
    case phrasetype: /* fall out to return stdfault */
        break;
    case faulttype:
        switch (kind(x1)) {
          case booltype:
              if (divflag && boolval(x1) == 0)
                break;
              freeup(stdfault);
              return (x0);
          case inttype:
              if (divflag && intval(x1) == 0)
                break;
              freeup(stdfault);
              return (x0);
          case realtype:
              if (divflag && realval(x1) == 0.0)
                break;
              freeup(stdfault);
              return (x0);
          case chartype:
          case phrasetype:
              break;
          case faulttype:
              if (x0 == x1) {
                freeup(stdfault);
                return (x0);
              }
              break;
        }
  }
  return (stdfault);
}

/* routine to convert an array to type integer */

static      nialptr
to_int(nialptr x)
{
  nialptr     z,
              xi;
  nialint     i,
              t = tally(x);
  int         v = valence(x);

  /* create the result container */
  z = new_create_array(inttype, v, 0, shpptr(x, v));
  for (i = 0; i < t; i++) {
    xi = fetch_array(x, i);
    if (kind(xi) == inttype) {
      copy1(z, i, xi, 0);
    }
    else   /* type must be boolean */
      store_int(z, i, boolval(xi));
  }
  return (z);
}


/* routine to convert an array to type real */

nialptr
to_real(nialptr x)
{
  nialptr     z,
              xi;
  nialint     i,
              t = tally(x);
  int         v = valence(x);
  double      r = 0.0;

  /* create the result container */
  z = new_create_array(realtype, v, 0, shpptr(x, v));
  for (i = 0; i < t; i++) {
    int         k;

    xi = fetch_array(x, i);
    k = kind(xi);
    if (k == realtype)
      r = *pfirstreal(xi);

    else  /* must be boolean or integer */ 
    if (k == inttype)
      r = 1.0 * intval(xi);

    else 
    if (k == booltype)
      r = 1.0 * boolval(xi);
    store_real(z, i, r);
  }
  return (z);
}

/* routine convert changes x to a higher numeric type k and
   resets kx. It returns false if k is not a numeric type.
 */

int
convert(nialptr * x, int *kx, int k)
{
  if (numeric(k)) {
    nialptr     z = Null;
    int         v = valence(*x);
    nialint     i,
                t = tally(*x);

    switch (k) {
      case inttype:          /* must be converting a booltype array */
          {
            nialint    *zptr;

            z = new_create_array(inttype, v, 0, shpptr(*x, v));
            zptr = pfirstint(z);  /* safe */
            for (i = 0; i < t; i++)
              *zptr++ = fetch_bool(*x, i);
          }
          break; 

      case realtype: /* converting a boolean or integer array */
          {
            double     *zptr;

            z = new_create_array(realtype, v, 0, shpptr(*x, v));
            zptr = pfirstreal(z); /* safe */
            if (*kx == booltype) {
              for (i = 0; i < t; i++)
                *zptr++ = (double) fetch_bool(*x, i);
            }
            else {
              nialint    *xptr = pfirstint(*x); /* safe */

              for (i = 0; i < t; i++)
                *zptr++ = (double) *xptr++;
            }
          }
          break;
    }
    freeup(*x);
    *x = z;
    *kx = kind(z);
    return true;
  }
  return false;
}


/* does dictionary comparison of phrases. returns -1, 0, or 1 to
   indicate x <, =, or > y.
*/

int
tkncompare(nialptr x, nialptr y)
{
  nialint     tx = tknlength(x),
              ty = tknlength(y),
              n = (tx <= ty ? tx : ty), /* length of shorter phrase */
              i = 0;
  char        cx,
              cy,
             *xptr = pfirstchar(x), /* safe */
             *yptr = pfirstchar(y); /* safe */

  /* loop comparing chars until they differ */
  do {
    cx = *xptr++;
    cy = *yptr++;
  }
  while (i++ < n && cx == cy);

  if (i > n) /* agree to length of shorter, result determined by length */
    return (tx == ty ? 0 : (tx < ty ? -1 : 1));

  /* otherwise result based on first character that differs */
  return (invseq[cx - LOWCHAR] < invseq[cy - LOWCHAR] ? -1 : 1);

}

/* routine to convert a boolean array to integer */

nialptr
boolstoints(nialptr x)
{
  nialptr     z;
  nialint     i,
              t = tally(x),
             *ptrz;
  int         v = valence(x);

  z = new_create_array(inttype, v, 0, shpptr(x, v));
  ptrz = pfirstint(z);       /* safe */
  for (i = 0; i < t; i++) {
    int         xi;

    xi = fetch_bool(x, i);
    *ptrz++ = xi;
  }
  return (z);
}

/* routine to convert a string to upper case. Does not copy it. */

void
cnvtup(char *str)
{
  int         i,
              n;

  n = strlen(str);
  for (i = 0; i < n; i++)
    str[i] = Upper((int)str[i]);
}

/* routine to convert a char, char string, or phrase x to a C array of chars
   in parameter value. The result is the length. */

nialint
ngetname(nialptr x, char *value)
{                            
  int         k = kind(x);

  if (k == chartype || k == phrasetype) {
    strcpy(value, pfirstchar(x));
    return (strlen(value));
  }
  else
    return (0);
}


/* routine to return a name in upper case.
  If the argument is empty, it returns grounded. */

nialptr
getuppername()
{
  nialptr     x = apop();
  int         n = ngetname(x, gcharbuf);

  freeup(x);
  if (n == 0)
    return (grounded);
  else {
    cnvtup(gcharbuf);
    return (makephrase(gcharbuf));
  }
}

/* routine to convert a string to lower case in a copy.
   Could use strdup().  Used by eval.c and profile.c */

char       *
slower(char *in_s)
{
  int         cnt = 0;
  char       *out_s = (char *) malloc(strlen(in_s) + 1);

  if (out_s == NULL)
    exit_cover1("Not enough memeory to continue", NC_FATAL);
  while (in_s[cnt] != '\0') {
    out_s[cnt] = Lower((int)in_s[cnt]);
    cnt++;
  }
  out_s[cnt] = '\0';
  return (out_s);
}
