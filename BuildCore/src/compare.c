/*==============================================================

  MODULE   COMPARE.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This module contains the comparison operation primitives including
       the pervasive comparators lt, lte, gt, gte, match and mate, 
       pervasive max and min, and 
       the general array comparators equal and up.

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

/* SJLIB */
#include <setjmp.h>


/* Q'Nial header files */

#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"
#include "compare.h"

#include "trs.h"             /* for int_each etc. */
#include "utils.h"           /* for converters */
#include "faults.h"          /* for logical fault */
#include "logicops.h"        /* for orbools and andbools */
#include "ops.h"             /* for simple and splifb */


/* declaration of internal static routines */

static nialint  maxints(nialint * ptrx, nialint n);
static int  maxchars(char *ptrx, nialint n);
static double maxreals(double *ptrx, nialint n);
static void maxintvectors(nialint * x, nialint * y, nialint * z, nialint n);
static void maxintscalarvector(nialint x, nialint * y, nialint * z, nialint n);
static void maxrealvectors(double *x, double *y, double *z, nialint n);
static void maxrealscalarvector(double x, double *y, double *z, nialint n);
static void maxcharvectors(char *x, char *y, char *z, nialint n);
static void maxcharscalarvector(char x, char *y, char *z, nialint n);
static int  minchars(char *ptrx, nialint n);
static nialint  minints(nialint * ptrx, nialint n);
static double minreals(double *ptrx, nialint n);
static void minintvectors(nialint * x, nialint * y, nialint * z, nialint n);
static void minintscalarvector(nialint x, nialint * y, nialint * z, nialint n);
static void minrealvectors(double *x, double *y, double *z, nialint n);
static void minrealscalarvector(double x, double *y, double *z, nialint n);
static void mincharvectors(char *x, char *y, char *z, nialint n);
static void mincharscalarvector(char x, char *y, char *z, nialint n);
static void fastcharcompare(nialptr x, nialptr y, nialptr z, nialint t, int code);
static void fastboolcompare(nialptr x, nialptr y, nialptr z, nialint t, int code);
static void fastintcompare(nialptr x, nialptr y, nialptr z, nialint t, int code);
static void fastrealcompare(nialptr x, nialptr y, nialptr z, nialint t, int code);
static void mateormatch(int matecase);

/* macros for comparison operations */
#define LTECODE 1
#define LTCODE 2
#define MATCHCODE 3

/* routine to implement the binary pervading operation max.
   This is called by imax when comparing pairs of arrays.
   It uses supplementary routines to permit vector processing
   if such support routines are available.
   */

void
b_max()
{
  nialptr     y = apop(),
              x = apop(),
              z;
  int         kx = kind(x),
              ky = kind(y);

  if (numeric(kx) && numeric(ky)) {
    if (kx < ky)
      convert(&x, &kx, ky);  /* convert x to y's type */
    else if (ky < kx)
      convert(&y, &ky, kx);  /* convert y to x's type */
  }
  if (kx == ky && homotype(kx)
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    switch (kx) {
      case booltype:         /* same as or */
          apush(x);
          apush(y);
          b_or();
          return;
      case inttype:
          {
            if (atomic(x)) {
              nialint     xv = intval(x);

              if (atomic(y)) {
                nialint     yv = intval(y);

                z = createint(xv >= yv ? xv : yv);
              }
              else {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(inttype, v, 0, shpptr(y, v));
                maxintscalarvector(intval(x), pfirstint(y), pfirstint(z), ty);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(inttype, v, 0, shpptr(x, v));
              if (atomic(y))
                maxintscalarvector(intval(y), pfirstint(x), pfirstint(z), tx);
              else
                maxintvectors(pfirstint(x), pfirstint(y), pfirstint(z), tx);
            }
          }
          break;
      case realtype:
          {
            if (atomic(x)) {
              double      xv = realval(x);

              if (atomic(y)) {
                double      yv = realval(y);

                z = createreal(xv >= yv ? xv : yv);
              }
              else {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(realtype, v, 0, shpptr(y, v));
                maxrealscalarvector(realval(x), pfirstreal(y), pfirstreal(z), ty);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(realtype, v, 0, shpptr(x, v));
              if (atomic(y))
                maxrealscalarvector(realval(y), pfirstreal(x), pfirstreal(z), tx);
              else
                maxrealvectors(pfirstreal(x), pfirstreal(y), pfirstreal(z), tx);
            }
          }
          break;
      case chartype:
          {
            if (atomic(x)) {
              int         xv = charval(x);

              if (atomic(y)) {
                int         yv = charval(y);

                z = createchar(invseq[xv - LOWCHAR] >=
                               invseq[yv - LOWCHAR] ? xv : yv);
              }
              else {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(chartype, v, 0, shpptr(y, v));
                maxcharscalarvector(charval(x), pfirstchar(y), pfirstchar(z), ty);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(chartype, v, 0, shpptr(x, v));
              if (atomic(y))
                maxcharscalarvector(charval(y), pfirstchar(x), pfirstchar(z), tx);
              else
                maxcharvectors(pfirstchar(x), pfirstchar(y), pfirstchar(z), tx);
            }
          }
          break;
      default:
          goto use_eachboth;
    }
  }
  else /* handle remaining cases */ 
   if (atomic(x) && atomic(y)) {
    if (kx == faulttype) {
      if (ky == faulttype) {
        if (x == y)
          z = x;
        else
          z = Zenith;
      }
      else if (x == Nadir)
        z = y;
      else
#ifdef V4AT
        z = Zenith;
#else
        z = x;
#endif
    }
    else if (ky == faulttype) {
      if (y == Nadir)
        z = x;
      else
#ifdef V4AT
        z = Zenith;
#else
        z = y;
#endif
    }
    else if (kx == phrasetype && ky == phrasetype) {
      nialptr     res;

      apush(x);
      apush(y);
      b_lt();
      res = apop();
      if (res == True_val)
        z = y;
      else
        z = x;
      freeup(res);
    }
    else                     /* chartype or phrasetype cause a fault */
      z = Zenith;
  }
  else {
use_eachboth:
    int_eachboth(b_max, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

void
imax()
{
  nialptr     x;
  nialint     tx;
  int         kx;

  x = apop();
  kx = kind(x);
  tx = tally(x);
  switch (kx) {
    case booltype:
        apush(createbool(orbools(x, tx)));
        break;
    case inttype:
        apush(createint(maxints(pfirstint(x), tx)));
        break;
    case realtype:
        apush(createreal(maxreals(pfirstreal(x), tx)));
        break;
    case chartype:
        apush(createchar(maxchars(pfirstchar(x), tx)));
        break;
    case phrasetype:
        apush(Zenith);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        if (tx == 0) {       /* create Nadir directly to avoid a trigger */
          apush(createatom(faulttype, "?O"));
        }
        else if (simple(x)) 
               /*  use binary case for consistency */
        { nialptr maxitem = fetch_array(x, 0);
          nialint i;
          apush(maxitem);
          for (i = 1; i < tx; i++) 
          { apush(fetch_array(x, i));
            b_max();
          }
        }
        else if (tx == 2) {
          apush(fetch_array(x, 0));
          apush(fetch_array(x, 1));
          b_max();
        }
        else {
          apush(x);
          ipack();
          int_each(imax, apop());
          return;
        }
        break;
  }
  freeup(x);
}

/* support routines for max. separated out so they can be calls
   to specialized routines for vector processors */

static nialint
maxints(nialint * ptrx, nialint n)
{
  nialint     i,
              it;
  nialint     s = *ptrx++;

  for (i = 1; i < n; i++) {
    it = *ptrx++;
    if (it > s)
      s = it;
  }
  return (s);
}

static int
maxchars(char *ptrx, nialint n)
{
  nialint     i;
  int         c;
  char        it,
              c1;

  c1 = *ptrx++;
  c = invseq[c1 - LOWCHAR];
  for (i = 1; i < n; i++) {
    it = *ptrx++;
    if (invseq[it - LOWCHAR] > c) {
      c = invseq[it - LOWCHAR];
      c1 = it;
    }
  }
  return (c1);
}

static double
maxreals(double *ptrx, nialint n)
{
  nialint     i;
  double      it,
              s = *ptrx++;

  for (i = 1; i < n; i++) {
    it = *ptrx++;
    if (it > s)
      s = it;
  }
  return (s);
}

static void
maxintvectors(nialint * x, nialint * y, nialint * z, nialint n)
{
  nialint     i,
              vx,
              vy;

  for (i = 0; i < n; i++) {
    vx = *x++;
    vy = *y++;
    *z++ = vx > vy ? vx : vy;
  }
}

static void
maxintscalarvector(nialint x, nialint * y, nialint * z, nialint n)
{
  nialint     i,
              vy;

  for (i = 0; i < n; i++) {
    vy = *y++;
    *z++ = x > vy ? x : vy;
  }
}

static void
maxrealvectors(double *x, double *y, double *z, nialint n)
{
  nialint     i;
  double      vx,
              vy;

  for (i = 0; i < n; i++) {
    vx = *x++;
    vy = *y++;
    *z++ = vx > vy ? vx : vy;
  }
}

static void
maxrealscalarvector(double x, double *y, double *z, nialint n)
{
  nialint     i;
  double      vy;

  for (i = 0; i < n; i++) {
    vy = *y++;
    *z++ = x > vy ? x : vy;
  }
}

static void
maxcharvectors(char *x, char *y, char *z, nialint n)
{
  nialint     i;
  char        vx,
              vy;

  for (i = 0; i < n; i++) {
    vx = *x++;
    vy = *y++;
    *z++ = invseq[vx - LOWCHAR] > invseq[vy - LOWCHAR] ? vx : vy;
  }
}

static void
maxcharscalarvector(char x, char *y, char *z, nialint n)
{
  nialint     i;
  char        vy;

  for (i = 0; i < n; i++) {
    vy = *y++;
    *z++ = invseq[x - LOWCHAR] > invseq[vy - LOWCHAR] ? x : vy;
  }
}

/*  routines to implement min. Same algorithm structure as max */


void
b_min()
{
  nialptr     y = apop(),
              x = apop(),
              z;
  int         kx = kind(x),
              ky = kind(y);

  if (numeric(kx) && numeric(ky)) {
    if (kx < ky)
      convert(&x, &kx, ky);  /* convert x to y's type */
    else if (ky < kx)
      convert(&y, &ky, kx);  /* convert y to x's type */
  }
  if (kx == ky && homotype(kx)
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    switch (kx) {
      case booltype:         /* same as and */
          apush(x);
          apush(y);
          b_and();
          return;
      case inttype:
          {
            if (atomic(x)) {
              nialint     xv = intval(x);

              if (atomic(y)) {
                nialint     yv = intval(y);

                z = createint(xv <= yv ? xv : yv);
              }
              else {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(inttype, v, 0, shpptr(y, v));
                minintscalarvector(intval(x), pfirstint(y), pfirstint(z), ty);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(inttype, v, 0, shpptr(x, v));
              if (atomic(y))
                minintscalarvector(intval(y), pfirstint(x), pfirstint(z), tx);
              else
                minintvectors(pfirstint(x), pfirstint(y), pfirstint(z), tx);
            }
          }
          break;
      case realtype:
          {
            if (atomic(x)) {
              double      xv = realval(x);

              if (atomic(y)) {
                double      yv = realval(y);

                z = createreal(xv <= yv ? xv : yv);
              }
              else {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(realtype, v, 0, shpptr(y, v));
                minrealscalarvector(realval(x), pfirstreal(y), pfirstreal(z), ty);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(realtype, v, 0, shpptr(x, v));
              if (atomic(y))
                minrealscalarvector(realval(y), pfirstreal(x), pfirstreal(z), tx);
              else
                minrealvectors(pfirstreal(x), pfirstreal(y), pfirstreal(z), tx);
            }
          }
          break;
      case chartype:
          {
            if (atomic(x)) {
              int         xv = charval(x);

              if (atomic(y)) {
                int         yv = charval(y);

                z = createchar(invseq[xv - LOWCHAR] <=
                               invseq[yv - LOWCHAR] ? xv : yv);
              }
              else {
                int         v = valence(y);
                nialint     ty = tally(y);

                z = new_create_array(chartype, v, 0, shpptr(y, v));
                mincharscalarvector(charval(x), pfirstchar(y), pfirstchar(z), ty);
              }
            }
            else {
              int         v = valence(x);
              nialint     tx = tally(x);

              z = new_create_array(chartype, v, 0, shpptr(x, v));
              if (atomic(y))
                mincharscalarvector(charval(y), pfirstchar(x), pfirstchar(z), tx);
              else
                mincharvectors(pfirstchar(x), pfirstchar(y), pfirstchar(z), tx);
            }
          }
          break;
      default:
          goto use_eachboth;
    }
  }
  else /* handle remaining cases */ 
  if (atomic(x) && atomic(y)) {
    if (kx == faulttype) {
      if (ky == faulttype) {
        if (x == y)
          z = x;
        else
          z = Nadir;
      }
      else {
        if (x == Zenith)
          z = y;
        else
#ifdef V4AT
          z = Nadir;
#else
          z = x;
#endif
      }
    }
    else if (ky == faulttype) {
      if (y == Zenith)
        z = x;
      else
#ifdef V4AT
        z = Nadir;
#else
        z = y;
#endif
    }
    else if (kx == phrasetype && ky == phrasetype) {
      nialptr     res;

      apush(x);
      apush(y);
      b_gt();
      res = apop();
      if (res == True_val)
        z = y;
      else
        z = x;
      freeup(res);
    }
    else                     /* chartype or phrasetype cause a fault */
      z = Nadir;
  }
  else {
use_eachboth:
    int_eachboth(b_min, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

void
imin()
{
  nialptr     x;
  nialint     tx;
  int         kx;

  x = apop();
  kx = kind(x);
  tx = tally(x);
  switch (kx) {
    case booltype:
        apush(createbool(andbools(x, tx)));
        break;
    case inttype:
        apush(createint(minints(pfirstint(x), tx)));
        break;
    case realtype:
        apush(createreal(minreals(pfirstreal(x), tx)));
        break;
    case chartype:
        apush(createchar(minchars(pfirstchar(x), tx)));
        break;
    case phrasetype:
        apush(Nadir);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        if (tx == 0) {       /* create Zenith directly to avoid a trigger */
          apush(createatom(faulttype, "?I"));
        }
        else if (simple(x)) 
         /* use binary case for consistency */
        { nialptr minitem = fetch_array(x, 0);
          nialint i;
          apush(minitem);
          for (i = 1; i < tx; i++) 
          { apush(fetch_array(x, i));
            b_min();
          }
        }
        else if (tx == 2) {
          apush(fetch_array(x, 0));
          apush(fetch_array(x, 1));
          b_min();
        }
        else {
          apush(x);
          ipack();
          int_each(imin, apop());
          return;
        }
        break;
  }
  freeup(x);
}


static int
minchars(char *ptrx, nialint n)
{
  nialint     i;
  int         c;
  char        it,
              c1;

  c1 = *ptrx++;
  c = invseq[c1 - LOWCHAR];
  for (i = 1; i < n; i++) {
    it = *ptrx++;
    if (invseq[it - LOWCHAR] < c) {
      c = invseq[it - LOWCHAR];
      c1 = it;
    }
  }
  return (c1);
}

static nialint
minints(nialint * ptrx, nialint n)
{
  nialint     i,
              it;
  nialint     s = *ptrx++;

  for (i = 1; i < n; i++) {
    it = *ptrx++;
    if (it < s)
      s = it;
  }
  return (s);
}

static double
minreals(double *ptrx, nialint n)
{
  nialint     i;
  double      it,
              s = *ptrx++;

  for (i = 1; i < n; i++) {
    it = *ptrx++;
    if (it < s)
      s = it;
  }
  return (s);
}

static void
minintvectors(nialint * x, nialint * y, nialint * z, nialint n)
{
  nialint     i,
              vx,
              vy;

  for (i = 0; i < n; i++) {
    vx = *x++;
    vy = *y++;
    *z++ = vx < vy ? vx : vy;
  }
}

static void
minintscalarvector(nialint x, nialint * y, nialint * z, nialint n)
{
  nialint     i,
              vy;

  for (i = 0; i < n; i++) {
    vy = *y++;
    *z++ = x < vy ? x : vy;
  }
}

static void
minrealvectors(double *x, double *y, double *z, nialint n)
{
  nialint     i;
  double      vx,
              vy;

  for (i = 0; i < n; i++) {
    vx = *x++;
    vy = *y++;
    *z++ = vx < vy ? vx : vy;
  }
}

static void
minrealscalarvector(double x, double *y, double *z, nialint n)
{
  nialint     i;
  double      vy;

  for (i = 0; i < n; i++) {
    vy = *y++;
    *z++ = x < vy ? x : vy;
  }
}

static void
mincharvectors(char *x, char *y, char *z, nialint n)
{
  nialint     i;
  char        vx,
              vy;

  for (i = 0; i < n; i++) {
    vx = *x++;
    vy = *y++;
    *z++ = invseq[vx - LOWCHAR] < invseq[vy - LOWCHAR] ? vx : vy;
  }
}

static void
mincharscalarvector(char x, char *y, char *z, nialint n)
{
  nialint     i;
  char        vy;

  for (i = 0; i < n; i++) {
    vy = *y++;
    *z++ = invseq[x - LOWCHAR] < invseq[vy - LOWCHAR] ? x : vy;
  }
}

#define typeno(a)  (numeric(a) ? 0 : kind(a))


/* routines to implement ilte (<=). 
   Same algorithmic structure as minus in arith.c */

void
b_lte()
{
  nialptr     y = apop(),
              x = apop(),
              z = Null;
  int         kx = kind(x),
              ky = kind(y);

  if (numeric(kx) && numeric(ky)) {
    if (kx < ky)
      convert(&x, &kx, ky);  /* convert x to y's type */
    else if (ky < kx)
      convert(&y, &ky, kx);  /* convert y to x's type */
  }
  if (atomic(x) && atomic(y)) {
    if (kx == ky) {
      switch (kx) {
        case booltype:
            z = createbool(boolval(x) <= boolval(y));
            break;
        case inttype:
            z = createbool(intval(x) <= intval(y));
            break;
        case realtype:
            z = createbool(realval(x) <= realval(y));
            break;
        case chartype:
            {
              int         c0 = invseq[charval(x) - LOWCHAR],
                          c1 = invseq[charval(y) - LOWCHAR];
              z = createbool(c0 <= c1);
            }
            break;
        case phrasetype:
            z = createbool(tkncompare(x, y) <= 0);
            break;
        case faulttype:
            {
              if (x == Nadir || y == Zenith || x == y)
                z = True_val;
              else if ((x == Zenith || y == Nadir) && x != y)
                z = False_val;
              else
                z = createbool(tkncompare(x, y) <= 0);
            }
            break;
      }
    }
    else {
      if (x == Nadir || y == Zenith)
        z = True_val;
      else if (x == Zenith || y == Nadir)
        z = False_val;
      else if (typeno(x) < typeno(y))
        z = True_val;
      else
        z = False_val;
    }
  }
  else if (kx == ky && homotype(kx) && (atomic(x) || atomic(y) || equalshape(x, y))) {
    nialint     t;
    int         v;

    if (atomic(x)) {
      v = valence(y);
      t = tally(y);
      z = new_create_array(booltype, v, 0, shpptr(y, v));
    }
    else {
      v = valence(x);
      t = tally(x);
      z = new_create_array(booltype, v, 0, shpptr(x, v));
    }
    switch (kx) {
      case booltype:
          fastboolcompare(x, y, z, t, LTECODE);
          break;
      case inttype:
          fastintcompare(x, y, z, t, LTECODE);
          break;
      case realtype:
          fastrealcompare(x, y, z, t, LTECODE);
          break;
      case chartype:
          fastcharcompare(x, y, z, t, LTECODE);
          break;
    }
  }
  else {
    int_eachboth(b_lte, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

/* unary version of lte */

void
ilte()
{
  nialptr     z;

  if (kind(top) == faulttype)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?<= expects a pair"));
  }
  else {
    nialptr     x,
                y;

    splitfb(z, &x, &y);
    apush(x);
    apush(y);
    b_lte();
  }
  freeup(z);
}

/* fast comparison routines for homogeneous arrays. Shared by
   lte and lt. */

static void
fastboolcompare(nialptr x, nialptr y, nialptr z, nialint t, int code)
{
  nialint     i;

  if (atomic(x)) {
    int         xv = boolval(x);

    for (i = 0; i < t; i++) {
      int         yv = fetch_bool(y, i);
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
  else if (atomic(y)) {
    int         yv = boolval(y);

    for (i = 0; i < t; i++) {
      int         xv = fetch_bool(x, i);
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
  else {
    int         xv,
                yv,
                zv = 0;

    for (i = 0; i < t; i++) {
      xv = fetch_bool(x, i);
      yv = fetch_bool(y, i);
      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
}

static void
fastintcompare(nialptr x, nialptr y, nialptr z, nialint t, int code)
{
  nialint     i;

  if (atomic(x)) {
    nialint     xv = intval(x);
    nialint    *yptr = pfirstint(y);  /* safe: no allocation */

    for (i = 0; i < t; i++) {
      nialint     yv = *yptr++;
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
  else if (atomic(y)) {
    nialint     yv = intval(y);
    nialint    *xptr = pfirstint(x);  /* safe: no allocation */

    for (i = 0; i < t; i++) {
      nialint     xv = *xptr++;
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
  else {
    nialint    *xptr = pfirstint(x);  /* safe: no allocation */
    nialint    *yptr = pfirstint(y);  /* safe: no allocation */

    for (i = 0; i < t; i++) {
      nialint     xv = *xptr++;
      nialint     yv = *yptr++;
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
}

static void
fastcharcompare(nialptr x, nialptr y, nialptr z, nialint t, int code)
{
  nialint     i;

  if (atomic(x)) {
    int         xv = invseq[charval(x) - LOWCHAR];
    char       *yptr = pfirstchar(y); /* safe: no allocation */

    for (i = 0; i < t; i++) {
      int         yv = invseq[*yptr++ - LOWCHAR];
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
  else if (atomic(y)) {
    int         yv = invseq[charval(y) - LOWCHAR];
    char       *xptr = pfirstchar(x); /* safe: no allocation */

    for (i = 0; i < t; i++) {
      int         xv = invseq[*xptr++ - LOWCHAR];
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
  else {
    char       *xptr = pfirstchar(x); /* safe: no allocation */
    char       *yptr = pfirstchar(y); /* safe: no allocation */

    for (i = 0; i < t; i++) {
      int         xv = invseq[*xptr++ - LOWCHAR];
      int         yv = invseq[*yptr++ - LOWCHAR];
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
}

static void
fastrealcompare(nialptr x, nialptr y, nialptr z, nialint t, int code)
{
  nialint     i;

  if (atomic(x)) {
    double      xv = realval(x);
    double     *yptr = pfirstreal(y); /* safe: no allocation */

    for (i = 0; i < t; i++) {
      double      yv = *yptr++;
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
  else if (atomic(y)) {
    double      yv = realval(y);
    double     *xptr = pfirstreal(x); /* safe: no allocation */

    for (i = 0; i < t; i++) {
      double      xv = *xptr++;
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
  else {
    double     *xptr = pfirstreal(x); /* safe: no allocation */
    double     *yptr = pfirstreal(y); /* safe: no allocation */

    for (i = 0; i < t; i++) {
      double      xv = *xptr++;
      double      yv = *yptr++;
      int         zv = 0;

      switch (code) {
        case LTECODE:
            zv = xv <= yv;
            break;
        case LTCODE:
            zv = xv < yv;
            break;
        case MATCHCODE:
            zv = xv == yv;
            break;
      }
      store_bool(z, i, zv);
    }
  }
}

/* routine to implement lt ( < less than ). 
   Same algorithmic structure as b_lte */


void
b_lt()
{
  nialptr     y = apop(),
              x = apop(),
              z = Null;
  int         kx = kind(x),
              ky = kind(y);

  if (numeric(kx) && numeric(ky)) {
    if (kx < ky)
      convert(&x, &kx, ky);  /* convert x to y's type */
    else if (ky < kx)
      convert(&y, &ky, kx);  /* convert y to x's type */
  }
  if (atomic(x) && atomic(y)) {
    if (kx == ky) {
      switch (kx) {
        case booltype:
            z = createbool(boolval(x) < boolval(y));
            break;
        case inttype:
            z = createbool(intval(x) < intval(y));
            break;
        case realtype:
            z = createbool(realval(x) < realval(y));
            break;
        case chartype:
            {
              int         c0 = invseq[charval(x) - LOWCHAR],
                          c1 = invseq[charval(y) - LOWCHAR];

              z = createbool(c0 < c1);
            }
            break;
        case phrasetype:
            z = createbool(tkncompare(x, y) < 0);
            break;
        case faulttype:
            {
              if (x == Nadir || y == Zenith)
                z = True_val;
              else if (x == Zenith || y == Nadir)
                z = False_val;
              else
                z = createbool(tkncompare(x, y) < 0);
            }
            break;
      }
    }
    else {
      if (x == Nadir || y == Zenith)
        z = True_val;
      else if (x == Zenith || y == Nadir)
        z = False_val;
      else if (typeno(x) < typeno(y))
        z = True_val;
      else
        z = False_val;
    }
  }
  else if (kx == ky && homotype(kx) && (atomic(x) || atomic(y) || equalshape(x, y))) {
    nialint     t;
    int         v;

    if (atomic(x)) {
      v = valence(y);
      t = tally(y);
      z = new_create_array(booltype, v, 0, shpptr(y, v));
    }
    else {
      v = valence(x);
      t = tally(x);
      z = new_create_array(booltype, v, 0, shpptr(x, v));
    }
    switch (kx) {
      case booltype:
          fastboolcompare(x, y, z, t, LTCODE);
          break;
      case inttype:
          fastintcompare(x, y, z, t, LTCODE);
          break;
      case realtype:
          fastrealcompare(x, y, z, t, LTCODE);
          break;
      case chartype:
          fastcharcompare(x, y, z, t, LTCODE);
          break;
    }
  }
  else {
    int_eachboth(b_lt, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

void
ilt()
{
  nialptr     z;

  if (kind(top) == faulttype)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?< expects a pair"));
  }
  else {
    nialptr     x,
                y;

    splitfb(z, &x, &y);
    apush(x);
    apush(y);
    b_lt();
  }
  freeup(z);
}

/* routine to implement mate (distributed equal with type coercions)
   or match (distributed equal without type coercions)
   */

void
b_mate()
{
  mateormatch(true);
}

void
b_match()
{
  mateormatch(false);
}

static void
mateormatch(int matecase)
{
  nialptr     y = apop(),
              x = apop(),
              z = Null;      /* initialized to avoid warning msg from case
                              * below */
  int         kx = kind(x),
              ky = kind(y);

  if (matecase && numeric(kx) && numeric(ky)) {
    if (kx < ky)
      convert(&x, &kx, ky);  /* convert x to y's type */
    else if (ky < kx)
      convert(&y, &ky, kx);  /* convert y to x's type */
  }
  if (atomic(x) && atomic(y)) {
    if (kx == ky) {
      switch (kx) {
        case booltype:
            z = createbool(boolval(x) == boolval(y));
            break;
        case inttype:
            z = createbool(intval(x) == intval(y));
            break;
        case realtype:
            z = createbool(realval(x) == realval(y));
            break;
        case chartype:
            {
              int         c0 = invseq[charval(x) - LOWCHAR],
                          c1 = invseq[charval(y) - LOWCHAR];

              z = createbool(c0 == c1);
            }
            break;
        case phrasetype:
        case faulttype:
            z = createbool(x == y);
            break;
      }
    }
    else
      z = False_val;
  }
  else if (kx == ky && homotype(kx) && (atomic(x) || atomic(y) || equalshape(x, y))) {
    nialint     t;
    int         v;

    if (atomic(x)) {
      v = valence(y);
      t = tally(y);
      z = new_create_array(booltype, v, 0, shpptr(y, v));
    }
    else {
      v = valence(x);
      t = tally(x);
      z = new_create_array(booltype, v, 0, shpptr(x, v));
    }
    switch (kx) {
      case booltype:
          fastboolcompare(x, y, z, t, MATCHCODE);
          break;
      case inttype:
          fastintcompare(x, y, z, t, MATCHCODE);
          break;
      case realtype:
          fastrealcompare(x, y, z, t, MATCHCODE);
          break;
      case chartype:
          fastcharcompare(x, y, z, t, MATCHCODE);
          break;
    }
  }
  else {
    if (matecase)
      int_eachboth(b_mate, x, y);
    else
      int_eachboth(b_match, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

void
imate()
{
  nialptr     z;

  if (kind(top) == faulttype)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?mate expects a pair"));
  }
  else {
    nialptr     x,
                y;

    splitfb(z, &x, &y);
    apush(x);
    apush(y);
    mateormatch(true);
  }
  freeup(z);
}

void
imatch()
{
  nialptr     z;

  if (kind(top) == faulttype)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?match expects a pair"));
  }
  else {
    nialptr     x,
                y;

    splitfb(z, &x, &y);
    apush(x);
    apush(y);
    mateormatch(false);
  }
  freeup(z);
}


/* >=  is implemented as CONVERSE <= */

void
b_gte()
{
  swap();
  b_lte();
}

void
igte()
{
  if (kind(top) == faulttype)
    return;
  if (tally(top) != 2) {
    freeup(apop());
    apush(makefault("?>= expects a pair"));
  }
  else {
    ireverse();
    ilte();
  }
}

/* > is implemented as CONVERSE < */

void
b_gt()
{
  swap();
  b_lt();
}

void
igt()
{
  if (kind(top) == faulttype)
    return;
  if (tally(top) != 2) {
    freeup(apop());
    apush(makefault("?> expects a pair"));
  }
  else {
    ireverse();
    ilt();
  }
}

/* The following pair of routines implement the Nial primitive equal.
  Two arrays are equal if they have the same shape and all the items are equal.
  If the argument is empty or only has one item then the result is true.
  If the argument has more than 2 items then the result is True if all the items 
  are the same.
  There are two versions that differ only when comparing two empty arrays.
  The flag V4AT selects code that returns True if the the empty arrays have the same shape
  and their prototypes are equal. 
  The V6AT result is True if the empty arrays have the same shape.  
 */

void
iequal()
{
  nialptr     z = apop();

  if (tally(z) <= 1) {
    apush(True_val);
  }
  else {
    int         result = true;
    nialint     t = tally(z),
                i = 1;
    nialptr     x = fetchasarray(z, 0);

    apush(x);                /* to preserve x across multiple calls of equal */
    while (result && i < t)
      result = equal(x, fetchasarray(z, i++));
    freeup(apop());          /* free protected x */
    apush(result ? True_val : False_val);
  }
  freeup(z);
}

/* equal assumes that all arrays are packed if possible */

int
equal(nialptr x, nialptr y)
{
  int         z;

  if (x == y)                /* same physical array */
    z = true;
  else {
    int         kx = kind(x),
                ky = kind(y);

    if (kx != ky)
      z = false;
    else {
      int         v = valence(x);

      if (v != valence(y))
        z = false;           /* Valence test fails */
      else {
        nialint    *shx = shpptr(x, v), /* Safe. used only in comparison */
                   *shy = shpptr(y, v),
                    i = 0;

        z = true;            /* assume equality */
        while (z && i++ < v) /* equality of shapes */
          z = *shx++ == *shy++;
        if (z) {             /* shapes are equal */
          nialint     t = tally(x);
#ifdef V4AT
          if (t==0)
            t = 1;  /* to test equality of the archetypes */
#endif

          /* test items for each type */
          switch (kx) {
            case atype:
                {
                  nialptr    *ptrx = (nialptr*)pfirstitem(x), /* safe in equal */
		    *ptry = (nialptr*)pfirstitem(y); /* safe in equal */

                  i = 0;
                  while (z && i++ < t)  /* test item equality using recursion */
                    z = equal(*ptrx++, *ptry++);
                  break;
                }
            case inttype:
                {
                  nialint    *ptrx = pfirstint(x),  /* safe in equal */
                             *ptry = pfirstint(y);  /* safe in equal */

                  i = 0;
                  while (z && i++ < t)
                    z = *ptrx++ == *ptry++;
                  break;
                }
            case chartype:
                {
                  char       *ptrx = pfirstchar(x), /* safe in equal */
                             *ptry = pfirstchar(y); /* safe in equal */

                  i = 0;
                  while (z && i++ < t)
                    z = *ptrx++ == *ptry++;
                  break;
                }
            case booltype:
                {
                  nialint    *ptrx = pfirstint(x),  /* safe in equal */
                             *ptry = pfirstint(y),  /* safe in equal */
                              limit = t / boolsPW;

                  i = 0;
                  while (z && i++ < limit)
                    z = *ptrx++ == *ptry++;
                  i = limit * boolsPW;  /* this part is unnecessary if we
                                         * guarantee the bits are set on all
                                         * constructions of booltype and the
                                         * above loop goes around one extra
                                         * time */
                  while (z && i < t) {
                    z = fetch_bool(x, i) == fetch_bool(y, i);
                    i++;
                  }
                  break;
                }
            case realtype:
                {
                  double     *ptrx = pfirstreal(x), /* safe in equal */
                             *ptry = pfirstreal(y); /* safe in equal */

                  i = 0;
                  while (z && i++ < t)
                    z = *ptrx++ == *ptry++;
                  break;
                }
            case phrasetype:
            case faulttype:
                z = (x == y);
                break;       /* since phrases and faults are unique */
          }
        }
      }
    }
  }
  freeup(x);
  freeup(y);
  return (z);
}


/* routines to implement the primitive comparator : up 
   which returns the lexicographic comparison of a and b. The ordering
   is based on comparing the items. If the arrays are atomic
   then the comparison is done on content if they are the same
   kind and on kind otherwise.
   The items are compared until they differ or one array is
   exhausted. The first items that differ are compared to decide
   the ordering. If the items all match then the ordering is
   decided on lexicographic ordering of their shapes.

*/

void
b_up()
{
  nialptr     y = apop(),
              x = apop();

  apush(createbool(up(x, y)));
}

void
iup(void)
{                            /* formerly called lexorder */
  nialptr     z,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of up must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    apush(createbool(up(x, y)));
  }
  freeup(z);
}

int
up(nialptr a, nialptr b)
{
  int         res = 0,
              cont,
              ka = kind(a),
              kb = kind(b);

  if (a == b)
    res = true;              /* they are the same object */
  else if (atomic(a) && atomic(b)) {
    if (ka != kb)
      res = ka < kb;
    else
      switch (ka) {          /* do directly rather than call b_lte() since
                              * the fault case differs. */
        case booltype:
            res = boolval(a) <= boolval(b);
            break;
        case inttype:
            res = intval(a) <= intval(b);
            break;
        case realtype:
            res = realval(a) <= realval(b);
            break;
        case chartype:
            {
              int         c0 = invseq[charval(a) - LOWCHAR],
                          c1 = invseq[charval(b) - LOWCHAR];

              res = c0 <= c1;
            }
            break;
        case phrasetype:     /* fall through to fault type */
        case faulttype:
            res = tkncompare(a, b) <= 0;
            break;
      }
  }
  else {
    nialint     tlya = tally(a),
                tlyb = tally(b),
                t = tlya < tlyb ? tlya : tlyb,
                i = 0;

    if (t == 0) {            /* have to handle empties first */
      if (tlya == 0 && tlyb == 0) /* both are empty */
        goto joinshape;
      else
        res = (tlya == 0);   /* true if a is empty, false if b is empty */
    }
    else if (ka != kb) {     /* no need to do item comparisons */
      if (homotype(ka) && homotype(kb))
        res = ka < kb;
      else {
        if (homotype(ka)) {
          a = explode(a, valence(a), tally(a), 0, tally(a));
          ka = atype;
        }
        if (homotype(kb)) {
          b = explode(b, valence(b), tally(b), 0, tally(b));
          kb = atype;
        }
        if (ka == atype && (kb == phrasetype || kb == faulttype)) {
          nialptr     it = fetch_array(a, 0);

          res = it == b ? false : up(it, b);  /* a match fails because of
                                               * valence */
        }
        else if (kb == atype && (ka == phrasetype || ka == faulttype))
          res = up(a, fetch_array(b, 0));
        else
          goto joinitems;
      }
    }
    else {                   /* arrays of same type class */

       /* compare items until they differ or are exhausted */
  joinitems:
      cont = true;
      while (cont && i < t) {
        switch (ka) {
          case booltype:
              cont = fetch_bool(a, i) == fetch_bool(b, i);
              break;
          case inttype:
              cont = fetch_int(a, i) == fetch_int(b, i);
              break;
          case realtype:
              cont = fetch_real(a, i) == fetch_real(b, i);
              break;
          case chartype:
              cont = invseq[fetch_char(a, i) - LOWCHAR] ==
                invseq[fetch_char(b, i) - LOWCHAR];
              break;
          case atype:
              cont = equal(fetch_array(a, i), fetch_array(b, i));
              break;
        }
        if (cont)
          i++;
      }
      if (!cont)             /* items differ. discriminate on one that
                              * differs */
        switch (ka) {
          case booltype:
              res = fetch_bool(a, i) < fetch_bool(b, i);
              break;
          case inttype:
              res = fetch_int(a, i) < fetch_int(b, i);
              break;
          case realtype:
              res = fetch_real(a, i) < fetch_real(b, i);
              break;
          case chartype:
              res = invseq[fetch_char(a, i) - LOWCHAR] <
                invseq[fetch_char(b, i) - LOWCHAR];
              break;
          case atype:
              res = up(fetch_array(a, i), fetch_array(b, i));
              break;
        }
      else if (tlya != tlyb)
        res = tlya < tlyb;
      else {
    joinshape:
        {
          int         va = valence(a),
                      vb = valence(b),
                      v;
          nialint    *shpa = shpptr(a, va), /* Safe since no heap creation */
                     *shpb = shpptr(b, vb);

          v = (va <= vb ? va : vb);
          cont = true;
          i = 0;
          while (cont && i < v) {
            cont = shpa[i] == shpb[i];
            if (cont)
              i++;
          }
          if (cont)
            res = va <= vb;
          else
            res = shpa[i] < shpb[i];
        }
      }
    }
  }

  freeup(a);
  freeup(b);
  return res;
}
