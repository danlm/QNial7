/*==============================================================

  MODULE   LOGICOPS.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This contains logic operation primitives.

================================================================*/



/* Q'Nial file that selects features */

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

/* STDLIB */
#include <stdlib.h>

/* STLIB */
#include <string.h>

/* SJLIB */
#include <setjmp.h>

/* CLIB */
#include <ctype.h>


/* Q'Nial header files */

#include "logicops.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"

#include "utils.h"           /* for converters */
#include "trs.h"             /* for int_each etc */
#include "ops.h"             /* for splitfb and simple */
#include "faults.h"          /* for Logical fault */

#include <limits.h>



/* declaration of internal static routines */

static void orboolvectors(nialptr x, nialptr y, nialptr z, nialint n);
static void xorboolvectors(nialptr x, nialptr y, nialptr z, nialint n);
static void andboolvectors(nialptr x, nialptr y, nialptr z, nialint n);
static void notbools(nialptr x, nialptr z, nialint n);
static void initbool(nialptr x, int v);


/*
 * Bitmasks to simplify boolean operations on vectors
 */

#ifdef INTS32
static nialint nialint_masks[33] = {
    0,
    0x1,
    0x3,
    0x7,
    0xf,
    0x1f,
    0x3f,
    0x7f,
    0xff,
    0x1ff,
    0x3ff,
    0x7ff,
    0xfff,
    0x1fff,
    0x3fff,
    0x7fff,
    0xffff,
    0x1ffff,
    0x3ffff,
    0x7ffff,
    0xfffff,
    0x1fffff,
    0x3fffff,
    0x7fffff,
    0xffffff,
    0x1ffffff,
    0x3ffffff,
    0x7ffffff,
    0xfffffff,
    0x1fffffff,
    0x3fffffff,
    0x7fffffff,
    0xffffffff
};
#endif
#ifdef INTS64
static nialint nialint_masks[65] = {
    0,
    0x1,
    0x3,
    0x7,
    0xf,
    0x1f,
    0x3f,
    0x7f,
    0xff,
    0x1ff,
    0x3ff,
    0x7ff,
    0xfff,
    0x1fff,
    0x3fff,
    0x7fff,
    0xffff,
    0x1ffff,
    0x3ffff,
    0x7ffff,
    0xfffff,
    0x1fffff,
    0x3fffff,
    0x7fffff,
    0xffffff,
    0x1ffffff,
    0x3ffffff,
    0x7ffffff,
    0xfffffff,
    0x1fffffff,
    0x3fffffff,
    0x7fffffff,
    0xffffffff,
    0x1ffffffff,
    0x3ffffffff,
    0x7ffffffff,
    0xfffffffff,
    0x1fffffffff,
    0x3fffffffff,
    0x7fffffffff,
    0xffffffffff,
    0x1ffffffffff,
    0x3ffffffffff,
    0x7ffffffffff,
    0xfffffffffff,
    0x1fffffffffff,
    0x3fffffffffff,
    0x7fffffffffff,
    0xffffffffffff,
    0x1ffffffffffff,
    0x3ffffffffffff,
    0x7ffffffffffff,
    0xfffffffffffff,
    0x1fffffffffffff,
    0x3fffffffffffff,
    0x7fffffffffffff,
    0xffffffffffffff,
    0x1ffffffffffffff,
    0x3ffffffffffffff,
    0x7ffffffffffffff,
    0xfffffffffffffff,
    0x1fffffffffffffff,
    0x3fffffffffffffff,
    0x7fffffffffffffff,
    0xffffffffffffffff
};
#endif


/* routine to implement the binary pervading operation or.
   This is called by ior when adding pairs of arrays.
   It uses supplementary routines to permit vector processing
   if such support routines are available.
*/

void
b_or()
{
  nialptr     y = apop(),
              x = apop(),
              z;
  int         kx = kind(x),
              ky = kind(y);

  if (kx == booltype && ky == booltype && (atomic(x) || atomic(y) || equalshape(x, y))) {
    if (atomic(x)) {
      if (atomic(y))
        z = createbool(boolval(x) || boolval(y));
      else if (boolval(x)) {
        /* x is l and result is all l */
        int         v = valence(y);

        z = new_create_array(booltype, v, 0, shpptr(y, v));
        initbool(z, 1);
      } else {
        /* y not atomic and x is o so result is y */
        z = y;
      }
    }
    else if (atomic(y)) {
      if (boolval(y)) {
        int         v = valence(x);

        z = new_create_array(booltype, v, 0, shpptr(x, v));
        initbool(z, 1);
      }
      else
        z = x;
    }
    else {
      int         v = valence(x);
      nialint     tx = tally(x);

      z = new_create_array(booltype, v, 0, shpptr(x, v));
      orboolvectors(x, y, z, tx);
    }
  }
  else
   /* handle remaining cases */ 
  if (atomic(x) && atomic(y)) 
  { if (kx == faulttype)
    { if (ky == faulttype)
      { if (x == y)
          z = x;
        else
          z = Logical;
      }
      else
        z = x;
    }
    else 
    if (ky == faulttype)
      z = y;
    else                     /* other types cause a fault */
      z = Logical;
  }
  else {
    int_eachboth(b_or, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

/* unary version of or */

void
ior()
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
    case chartype:
    case realtype:
    case phrasetype:
        apush(Logical);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        if (tx == 0)
        {
          apush(False_val);
        }
        else if (simple(x)) {/* has non-numeric items */
          apush(testfaults(x, Logical));
        }
        else if (tx == 2) {
          apush(fetch_array(x, 0));
          apush(fetch_array(x, 1));
          b_or();
        }
        else {
          apush(x);
          ipack();
          int_each(ior, apop());
          return;
        }
        break;
  }
  freeup(x);
}

/* support routines for or 
  Does it word by word except for partial word at the end 
*/

int
orbools(nialptr x, nialint n)
{
  nialint     i = 0,
             *ptrx = pfirstint(x),  /* safe: no allocations */
              s = false,
              wds = n / boolsPW,
              exc = n % boolsPW;

  /* handle leading words */
  while (!s && i++ < wds)
    s = ((*ptrx++) != 0);        /* at least one bit is on */
    
  /* handle trailing bits */
  if (s == false && exc != 0) {
    nialint mask = ~nialint_masks[boolsPW-exc];
    s = ((*ptrx & mask)) != 0;
  }

  return (s);
}

static void
orboolvectors(nialptr x, nialptr y, nialptr z, nialint n)
{
  nialint     i,
             *ptrx = pfirstint(x),  /* safe: no allocations */
             *ptry = pfirstint(y),  /* safe: no allocations */
             *ptrz = pfirstint(z),  /* safe: no allocations */
              wds = n / boolsPW,    /* initial segment of full words */
              exc = n % boolsPW;    /* excess bits over full words */

  for (i = 0; i < wds; i++)
    *ptrz++ = *ptrx++ | *ptry++;
  if (exc != 0)
    *ptrz++ = (*ptrx++ | *ptry++) & ~nialint_masks[boolsPW-exc];
}

/* routine to implement the binary pervading operation xor.
   This is called by ixor when adding pairs of arrays.
   It uses supplementary routines to permit vector processing
   if such support routines are available.
*/

void
b_xor()
{
  nialptr     y = apop(),
              x = apop(),
              z;
  int         kx = kind(x),
              ky = kind(y);

  if (kx == booltype && ky == booltype && (atomic(x) || atomic(y) || equalshape(x, y))) {
    /* Strict realm of conformant bools */
    if (atomic(x) && atomic(y)) {
        z = createbool(boolval(x) ^ boolval(y));
    } else if (atomic(x) || atomic(y)) {
        /* We will either copy the bits or their complement of the non-atomic argument
         * depending on the value of the atomic argument
         */
        int v;
        nialptr s;
        nialint bval, i, wc, exc, cnt, *zptr, *sptr;
        
        if (atomic(x)) {
            bval = boolval(x);
            s = y;
        } else {
            bval = boolval(y);
            s = x;
        }
        
        v = valence(s);
        z = new_create_array(booltype, v, 0, shpptr(s, v));

        cnt  = tally(s);             /* total bits */
        wc   = cnt/boolsPW;          /* unmasked word count */
        exc  = cnt%boolsPW;          /* extra bit segment   */
        zptr = pfirstint(z);
        sptr = pfirstint(s);
        
        if (bval) {
            /* Atomic value is true(l) so flip bits of source for xor */
            for (i = 0; i < wc; i++)
                *zptr++ = ~*sptr++;
            if (exc != 0)
                *zptr++ = (~*sptr++) & ~nialint_masks[boolsPW-exc];
        } else {
            /* Atomic value is false(o) so copy bits of source for xor */
            for (i = 0; i < wc; i++)
                *zptr++ = *sptr++;
            if (exc != 0)
                *zptr++ = (*sptr++) & ~nialint_masks[boolsPW-exc];
         }

    } else {
        /* Neither argument is atomic but they are conformant */
        int         v = valence(x);
        nialint     tx = tally(x);

        z = new_create_array(booltype, v, 0, shpptr(x, v));
        xorboolvectors(x, y, z, tx);
    }
  } else
   /* handle remaining cases */ 
  if (atomic(x) && atomic(y)) 
  { if (kx == faulttype)
    { if (ky == faulttype)
      { if (x == y)
          z = x;
        else
          z = Logical;
      }
      else
#ifdef V4AT
        z = Logical;
#else
        z = x;
#endif
    }
    else 
    if (ky == faulttype)
#ifdef V4AT
      z = Logical;
#else
      z = y;
#endif
    else                     /* other types cause a fault */
      z = Logical;
  }
  else {
    int_eachboth(b_xor, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

/* unary version of xor */

void
ixor()
{
  nialptr     x;
  nialint     tx;
  int         kx;

  x = apop();
  kx = kind(x);
  tx = tally(x);
  switch (kx) {
    case booltype:
        apush(createbool(xorbools(x, tx)));
        break;
    case inttype:
    case chartype:
    case realtype:
    case phrasetype:
        apush(Logical);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        if (tx == 0)
#ifdef V4AT
        { nialptr archetype = fetch_array(x,0);
          if (atomic(archetype))
          { if (kind(archetype)==booltype)
              { apush(False_val); }
            else
              apush(Logical);
          }
          else
          { apush(x);
            ipack();
            int_each(ixor,apop());
            return;
          }
        }
#else
        {
          apush(False_val);
        }
#endif
        else if (simple(x)) {/* has non-numeric items */
          apush(testfaults(x, Logical));
        }
        else if (tx == 2) {
          apush(fetch_array(x, 0));
          apush(fetch_array(x, 1));
          b_xor();
        }
        else {
          apush(x);
          ipack();
          int_each(ixor, apop());
          return;
        }
        break;
  }
  freeup(x);
}

/* support routines for xor 
  Does it word by word except for partial word at the end 
*/

int
xorbools(nialptr x, nialint n)
{
  nialint     i = 0, j = 0, bpos = 0, cw,
             *ptrx = pfirstint(x),  /* safe: no allocations */
              s = false,
              wds = n/ boolsPW,
              exc = n % boolsPW;
  
  /*
   * exclusive or is associative and commutative.
   * We can xor all the words then xor bits in result.
   */
  for (i = 0; i < wds; i++) 
    s ^= *ptrx++;
    
  /*
   * xor in bits of trailer. The zeros after masking
   * will not affect result as ^0  is identity
   */
  if (exc != 0)
    s ^= ((*ptrx) & ~nialint_masks[boolsPW-exc]);

  /*
  * Now fold the bits of the word with xor.
  * we only care about the last bit so dont need masks
  */
  for (j = boolsPW/2; j > 0; j >>= 1)
    s = s ^ (s >> j);
    
  return (s&1);
}


static void
xorboolvectors(nialptr x, nialptr y, nialptr z, nialint n)
{
  nialint     i,
             *ptrx = pfirstint(x),  /* safe: no allocations */
             *ptry = pfirstint(y),  /* safe: no allocations */
             *ptrz = pfirstint(z),  /* safe: no allocations */
              wds = n / boolsPW,    /* initial segment of full words */
              exc = n % boolsPW;    /* excess bits over full words */

  for (i = 0; i < wds; i++)
    *ptrz++ = *ptrx++ ^ *ptry++;
  if (exc != 0)
    *ptrz++ = (*ptrx++ ^ *ptry++) & ~nialint_masks[boolsPW-exc];
}

static void
initbool(nialptr x, int v)
{
  nialint     i,
             *ptrx = pfirstint(x),  /* safe: no allocations */
              wv = (v ? ALLBITSON : 0),
              n = tally(x),
              wds = n / boolsPW;

  for (i = 0; i < wds; i++)
    *ptrx++ = wv;
  for (i = wds * boolsPW; i < n; i++)
    store_bool(x, i, v);
}

/* routines to implement and. Same algorithm as ior */

void
b_and()
{
  nialptr     y = apop(),
              x = apop(),
              z;
  int         kx = kind(x),
              ky = kind(y);

  if (kx == booltype && ky == booltype
      && (atomic(x) || atomic(y) || equalshape(x, y))) {
    if (atomic(x)) {
      if (atomic(y))
        z = createbool(boolval(x) && boolval(y));
      else if (boolval(x))
        z = y;
      else {
        int         v = valence(y);

        z = new_create_array(booltype, v, 0, shpptr(y, v));
        initbool(z, 0);
      }
    }
    else if (atomic(y)) {
      if (boolval(y))
        z = x;
      else {
        int         v = valence(x);

        z = new_create_array(booltype, v, 0, shpptr(x, v));
        initbool(z, 0);
      }
    }
    else {
      int         v = valence(x);
      nialint     tx = tally(x);

      z = new_create_array(booltype, v, 0, shpptr(x, v));
      andboolvectors(x, y, z, tx);
    }
  }
  else
   /* handle remaining cases */ 
  if (atomic(x) && atomic(y)) 
  { if (kx == faulttype)
    { if (ky == faulttype)
      { if (x == y)
           z = x;
         else
           z = Logical;
      }
      else
        z = x;
    }
    else 
    if (ky == faulttype)
      z = y;
    else                     /* other types cause a fault */
      z = Logical;
  }
  else {
    int_eachboth(b_and, x, y);
    return;
  }
  apush(z);
  freeup(x);
  freeup(y);
}

void
iand()
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
    case chartype:
    case realtype:
    case phrasetype:
        apush(Logical);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        if (tx == 0)
        {
          apush(True_val);
        }
        else if (simple(x)) {/* has non-numeric items */
          apush(testfaults(x, Logical));
        }
        else if (tx == 2) {
          apush(fetch_array(x, 0));
          apush(fetch_array(x, 1));
          b_and();
        }
        else {
          apush(x);
          ipack();
          int_each(iand, apop());
          return;
        }
        break;
  }
  freeup(x);
}

/* support routines for and. 
   Does it word by word except for partial word at the end 
*/

int
andbools(nialptr x, nialint n)
{
  nialint     i = 0, cw,
             *ptrx = pfirstint(x),  /* safe: no allocations */
              s = true,
              wds = n / boolsPW,
              exc = n % boolsPW;

  /* Handle leading words */
  while (s && i++ < wds)
    s = (*ptrx++ == ALLBITSON);/* all bits are on */
  
  /* handle trailing bits */ 
  if (s && exc != 0) {
    nialint mask = ~nialint_masks[boolsPW-exc];
    s = (*ptrx & mask) == mask;
  }
  return (s);
}

static void
andboolvectors(nialptr x, nialptr y, nialptr z, nialint n)
{
  nialint     i,
             *ptrx = pfirstint(x),  /* safe: no allocations */
             *ptry = pfirstint(y),  /* safe: no allocations */
             *ptrz = pfirstint(z),  /* safe: no allocations */
              wds = n / boolsPW,
              exc = n % boolsPW;
              
  for (i = 0; i < wds; i++)
    *ptrz++ = *ptrx++ & *ptry++;
  if (exc != 0)
    *ptrz++ = (*ptrx++ & *ptry++) & ~nialint_masks[boolsPW-exc];
}

/* routine to implement not. Same algorithm as abs in arith.c */

void
inot()
{
  nialptr     z,
              x = apop();
  int         k = kind(x),
              v = valence(x);
  nialint     t = tally(x);

  switch (k) {
    case booltype:
        z = new_create_array(booltype, v, 0, shpptr(x, v));
        notbools(x, z, t);
        apush(z);
        freeup(x);
        break;
    case inttype:
    case realtype:
    case chartype:
        if (atomic(x)) {
          apush(Logical);
          freeup(x);
        }
        else
          int_each(inot, x);
        break;
    case phrasetype:
        apush(Logical);
        break;
    case faulttype:
        apush(x);
        break;
    case atype:
        int_each(inot, x);
        break;
  }
}

/* support routines for not 
   Does it word by word except for partial word at end.
*/

static void
notbools(nialptr x, nialptr z, nialint n)
{
  nialint     i,
              wds = n / boolsPW,
              exc = n % boolsPW,
             *ptrx = pfirstint(x),  /* safe: no allocations */
             *ptrz = pfirstint(z);  /* safe: no allocations */

  for (i = 0; i < wds; i++)
    *ptrz++ = ~(*ptrx++);
  if (exc != 0)
    *ptrz++ = (~(*ptrx++)) & ~nialint_masks[boolsPW-exc];
}

/**
 * The following is a precomputed table for accumulating xor
 * using bytes.
 */

static unsigned char acc_xor_table0[256] = {
   /* 00 */  0x00, 0x01, 0x03, 0x02, 0x07, 0x06, 0x04, 0x05,
   /* 08 */  0x0f, 0x0e, 0x0c, 0x0d, 0x08, 0x09, 0x0b, 0x0a,
   /* 10 */  0x1f, 0x1e, 0x1c, 0x1d, 0x18, 0x19, 0x1b, 0x1a,
   /* 18 */  0x10, 0x11, 0x13, 0x12, 0x17, 0x16, 0x14, 0x15,
   /* 20 */  0x3f, 0x3e, 0x3c, 0x3d, 0x38, 0x39, 0x3b, 0x3a,
   /* 28 */  0x30, 0x31, 0x33, 0x32, 0x37, 0x36, 0x34, 0x35,
   /* 30 */  0x20, 0x21, 0x23, 0x22, 0x27, 0x26, 0x24, 0x25,
   /* 38 */  0x2f, 0x2e, 0x2c, 0x2d, 0x28, 0x29, 0x2b, 0x2a,
   /* 40 */  0x7f, 0x7e, 0x7c, 0x7d, 0x78, 0x79, 0x7b, 0x7a,
   /* 48 */  0x70, 0x71, 0x73, 0x72, 0x77, 0x76, 0x74, 0x75,
   /* 50 */  0x60, 0x61, 0x63, 0x62, 0x67, 0x66, 0x64, 0x65,
   /* 58 */  0x6f, 0x6e, 0x6c, 0x6d, 0x68, 0x69, 0x6b, 0x6a,
   /* 60 */  0x40, 0x41, 0x43, 0x42, 0x47, 0x46, 0x44, 0x45,
   /* 68 */  0x4f, 0x4e, 0x4c, 0x4d, 0x48, 0x49, 0x4b, 0x4a,
   /* 70 */  0x5f, 0x5e, 0x5c, 0x5d, 0x58, 0x59, 0x5b, 0x5a,
   /* 78 */  0x50, 0x51, 0x53, 0x52, 0x57, 0x56, 0x54, 0x55,
   /* 80 */  0xff, 0xfe, 0xfc, 0xfd, 0xf8, 0xf9, 0xfb, 0xfa,
   /* 88 */  0xf0, 0xf1, 0xf3, 0xf2, 0xf7, 0xf6, 0xf4, 0xf5,
   /* 90 */  0xe0, 0xe1, 0xe3, 0xe2, 0xe7, 0xe6, 0xe4, 0xe5,
   /* 98 */  0xef, 0xee, 0xec, 0xed, 0xe8, 0xe9, 0xeb, 0xea,
   /* a0 */  0xc0, 0xc1, 0xc3, 0xc2, 0xc7, 0xc6, 0xc4, 0xc5,
   /* a8 */  0xcf, 0xce, 0xcc, 0xcd, 0xc8, 0xc9, 0xcb, 0xca,
   /* b0 */  0xdf, 0xde, 0xdc, 0xdd, 0xd8, 0xd9, 0xdb, 0xda,
   /* b8 */  0xd0, 0xd1, 0xd3, 0xd2, 0xd7, 0xd6, 0xd4, 0xd5,
   /* c0 */  0x80, 0x81, 0x83, 0x82, 0x87, 0x86, 0x84, 0x85,
   /* c8 */  0x8f, 0x8e, 0x8c, 0x8d, 0x88, 0x89, 0x8b, 0x8a,
   /* d0 */  0x9f, 0x9e, 0x9c, 0x9d, 0x98, 0x99, 0x9b, 0x9a,
   /* d8 */  0x90, 0x91, 0x93, 0x92, 0x97, 0x96, 0x94, 0x95,
   /* e0 */  0xbf, 0xbe, 0xbc, 0xbd, 0xb8, 0xb9, 0xbb, 0xba,
   /* e8 */  0xb0, 0xb1, 0xb3, 0xb2, 0xb7, 0xb6, 0xb4, 0xb5,
   /* f0 */  0xa0, 0xa1, 0xa3, 0xa2, 0xa7, 0xa6, 0xa4, 0xa5,
   /* f8 */  0xaf, 0xae, 0xac, 0xad, 0xa8, 0xa9, 0xab, 0xaa
};

   
static unsigned char acc_xor_table1[256] = {
   /* 00 */  0xff, 0xfe, 0xfc, 0xfd, 0xf8, 0xf9, 0xfb, 0xfa,
   /* 08 */  0xf0, 0xf1, 0xf3, 0xf2, 0xf7, 0xf6, 0xf4, 0xf5,
   /* 10 */  0xe0, 0xe1, 0xe3, 0xe2, 0xe7, 0xe6, 0xe4, 0xe5,
   /* 18 */  0xef, 0xee, 0xec, 0xed, 0xe8, 0xe9, 0xeb, 0xea,
   /* 20 */  0xc0, 0xc1, 0xc3, 0xc2, 0xc7, 0xc6, 0xc4, 0xc5,
   /* 28 */  0xcf, 0xce, 0xcc, 0xcd, 0xc8, 0xc9, 0xcb, 0xca,
   /* 30 */  0xdf, 0xde, 0xdc, 0xdd, 0xd8, 0xd9, 0xdb, 0xda,
   /* 38 */  0xd0, 0xd1, 0xd3, 0xd2, 0xd7, 0xd6, 0xd4, 0xd5,
   /* 40 */  0x80, 0x81, 0x83, 0x82, 0x87, 0x86, 0x84, 0x85,
   /* 48 */  0x8f, 0x8e, 0x8c, 0x8d, 0x88, 0x89, 0x8b, 0x8a,
   /* 50 */  0x9f, 0x9e, 0x9c, 0x9d, 0x98, 0x99, 0x9b, 0x9a,
   /* 58 */  0x90, 0x91, 0x93, 0x92, 0x97, 0x96, 0x94, 0x95,
   /* 60 */  0xbf, 0xbe, 0xbc, 0xbd, 0xb8, 0xb9, 0xbb, 0xba,
   /* 68 */  0xb0, 0xb1, 0xb3, 0xb2, 0xb7, 0xb6, 0xb4, 0xb5,
   /* 70 */  0xa0, 0xa1, 0xa3, 0xa2, 0xa7, 0xa6, 0xa4, 0xa5,
   /* 78 */  0xaf, 0xae, 0xac, 0xad, 0xa8, 0xa9, 0xab, 0xaa,
   /* 80 */  0x00, 0x01, 0x03, 0x02, 0x07, 0x06, 0x04, 0x05,
   /* 88 */  0x0f, 0x0e, 0x0c, 0x0d, 0x08, 0x09, 0x0b, 0x0a,
   /* 90 */  0x1f, 0x1e, 0x1c, 0x1d, 0x18, 0x19, 0x1b, 0x1a,
   /* 98 */  0x10, 0x11, 0x13, 0x12, 0x17, 0x16, 0x14, 0x15,
   /* a0 */  0x3f, 0x3e, 0x3c, 0x3d, 0x38, 0x39, 0x3b, 0x3a,
   /* a8 */  0x30, 0x31, 0x33, 0x32, 0x37, 0x36, 0x34, 0x35,
   /* b0 */  0x20, 0x21, 0x23, 0x22, 0x27, 0x26, 0x24, 0x25,
   /* b8 */  0x2f, 0x2e, 0x2c, 0x2d, 0x28, 0x29, 0x2b, 0x2a,
   /* c0 */  0x7f, 0x7e, 0x7c, 0x7d, 0x78, 0x79, 0x7b, 0x7a,
   /* c8 */  0x70, 0x71, 0x73, 0x72, 0x77, 0x76, 0x74, 0x75,
   /* d0 */  0x60, 0x61, 0x63, 0x62, 0x67, 0x66, 0x64, 0x65,
   /* d8 */  0x6f, 0x6e, 0x6c, 0x6d, 0x68, 0x69, 0x6b, 0x6a,
   /* e0 */  0x40, 0x41, 0x43, 0x42, 0x47, 0x46, 0x44, 0x45,
   /* e8 */  0x4f, 0x4e, 0x4c, 0x4d, 0x48, 0x49, 0x4b, 0x4a,
   /* f0 */  0x5f, 0x5e, 0x5c, 0x5d, 0x58, 0x59, 0x5b, 0x5a,
   /* f8 */  0x50, 0x51, 0x53, 0x52, 0x57, 0x56, 0x54, 0x55
};




/**
 * The following implements a pimitive version of 'accumulate xor' for
 * faster bit manipulation. The input is always a boolean array.
 */
void iacc_xor(void) {
  nialptr x = apop();
  int v = valence(x);
  nialint bc = tally(x);               /* bitcount */
  nialint i, j, wdc = (bc+boolsPW-1)/boolsPW, exc=bc%boolsPW;
  nialptr res;              /* Result object */
  unialint *px;     /* source */
  unialint *pz;     /* destination */
  unialint xch;
  int acc = 0;  /* Carry from byte to byte */

  /* Only work with boolean array */
  if (kind(x) != booltype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  if (bc <= 1) {
    apush(createbool(boolval(x)));
  } else {
    
    /* Build the result array */
    res = new_create_array(booltype, v, 0, shpptr(x,v));
       
    px = (unialint *)(pfirstint(x));
    pz = (unialint *)(pfirstint(res));
    
    /* Accumulate xor bits a word at a time */
    for(i = 0; i < wdc; i++) {
        unialint cwd = *px++;
        unialint rwd = 0;
        
        for (j = boolsPW-8; j >= 0; j -= 8) {
            nialint ch = (cwd >> j)&0xFF;
            xch = (acc == 0)? acc_xor_table0[ch]: acc_xor_table1[ch]; 
            rwd = (rwd << 8) | xch;
            acc = xch&1;
        }
        
        *pz++ = rwd;
    }
    
    /* Remove all but the trailing bits */
    if (exc != 0) {
        pz--;
        *pz &= (ALLBITSON << (boolsPW - exc));
    }

    apush(res);
  }
  
  freeup(x);
  return;
}