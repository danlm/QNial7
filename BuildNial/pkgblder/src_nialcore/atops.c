/*==============================================================

  MODULE   ATOPS.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This contains array theory operation primitives.

================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

/* standard library header files */

/* CLIB */
#include <ctype.h>

/* STLIB */
#include <string.h>

/* IOLIB */
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#ifdef UNIXSYS
#include <sys/mman.h>
#endif
#include <sys/fcntl.h>

/* SJLIB */
#include <setjmp.h>

/* STDLIB */
#include <stdlib.h>

/* LIMITLIB */
#include <limits.h>


/* Q'Nial header files */

#include "qniallim.h"
#include "absmach.h"
#include "ops.h"
#include "basics.h"
#include "lib_main.h"
#include "compare.h"         /* for equal and up */
#include "arith.h"           /* for sumints and prodints */
#include "trs.h"             /* for int_each */
#include "if.h"              /* for checksignal */
#include "utils.h"           /* for tkncompare */
#include "insel.h"           /* for choose */
#include "fileio.h"          /* for nprintf */




static nialptr gage(nialptr x);

static void cutorcutall(nialptr x, nialptr y, int cutprim);
static void nial_solitary(nialptr x);
static void takedrop(nialptr x, nialptr y, int istake);
static void nseek(nialptr x, nialptr y);
static void pack(nialptr x);
static int  validshape(nialptr x);
static void sublist(nialptr x, nialptr y);
static void sfindallint(nialptr x, nialptr y, int firstonly);
static void sfindallreal(nialptr x, nialptr y, int firstonly);
static void sfindallatype(nialptr x, nialptr y, int firstonly);
static void except(nialptr a, nialptr b);
static void oldexcept(nialptr a, nialptr b);
static void sexcept(nialptr a, nialptr b);
static void fuse(nialptr a, nialptr b);
static void scull(nialptr a, int diversesw);

/* global variables used to turn on debugging selectively */
extern int doprintf;
extern nialptr globalx;

/* routine to implement the operation "shape". The result gives
   the lengths of the axes as stored in the representation for
   an array. The result is that the shape of a list
   is the solitary of the integer. */

void
ishape()
{
  nialptr     x = apop();
  nialint   i,
              v = valence(x);
  if (v==0)
    { apush(Null); }
  else
  /* shape is always a list */
    { nialptr z;
      nialint *shptr;
      z = new_create_array(inttype, 1, 0, &v);
      shptr = shpptr(x, v); /* Safe. done after create. */
    for (i = 0; i < v; i++)
      store_int(z, i, shptr[i]);
    apush(z);
  }
  freeup(x);
}


/* implements first, which returns the first item of the array.
 
   The result is the fault ?address for an empty array.

   Array Theory Rules:
     atomic A => first A = A
     nonempty A => first A = 0 pick list A
     empty A => first A = ??address

   Note: This routine belongs with the other selection operations in
   module insel.c . It is here for historical reasons since Trenchard
   More treated it as one of his 8 primitives in his early versions of
   array theory.
*/

void
ifirst()
{
  nialptr     x,
              z;

  x = apop();
  if (atomic(x))
    z = x;
  else
  if (tally(x) > 0)
    z = fetchasarray(x, 0);
  else
    z = makefault("?address");
  apush(z);
  freeup(x);
}


 /* implements solitary, which returns a one-item list with the argument as its item. */

static void
nial_solitary(nialptr x)
{
  nialptr     z;
  nialint     sz = 1;
  int         kx = kind(x);

  if (homotype(kx) && atomic(x)) {
    z = new_create_array(kx, 1, 0, &sz);
    copy1(z, 0, x, 0);
    freeup(x);
  }
  else {
    z = new_create_array(atype, 1, 0, &sz);
    store_array(z, 0, x);
  }
  if (is_sorted(x))
    set_sorted(z, true);
  apush(z);
}

void
isolitary()
{
  nial_solitary(apop());
}

/* implements rest, which returns the list of all but the first items of the argument.
   The result is the empty list Null when the argument has only one item.
*/

void
irest()
{
  nialptr     x,
              z;

  x = apop();
  {
    nialint     zt,
                t = tally(x);

    zt = (t > 0 ? t - 1 : 0);
    if (zt == 0)
      z = Null;
    else {
      z = new_create_array(kind(x), 1, 0, &zt);
      copy(z, 0, x, 1, zt);
    }
  }
  if (homotest(z))
    z = implode(z);
  if (is_sorted(x))
    set_sorted(z, true);
  apush(z);
  freeup(x);
}

/* the following routines implement the binary operation append.
 The result is a list of length one longer than the first argument with the items
  of the first argument followed by the second argument as the last last item.
*/

void
b_append()
{
  nialptr     y = apop(),
              x = apop();

  append(x, y);
}

void
iappend()
{
  nialptr     x,
              y,
              z;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of append must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    append(x, y);
  }
  freeup(z);
}

void
append(nialptr x, nialptr y)
{
  nialptr     z;
  nialint     n,
              n1;
  int         kx,
              ky,
              newkind;

  kx = kind(x);
  ky = kind(y);
  n = tally(x);
  if (n == 0) {              /* append on an empty creates a solitary */
    nial_solitary(y);        /* using this code avoids needing an implode
                              * below */
    freeup(x);
    return;
  }
  if (atomic(x)) {           /* this code needed for faults and phrases at
                              * least */
    pair(x, y);
    return;                  /* pair will store x and y or free them */
  }
  if ((double) n + 1 > LARGEINT)
    exit_cover1("integer overflow in append", NC_WARNING);
  n1 = n + 1;
  if ((kx == ky && valence(y) == 0) || kx == atype) {
    newkind = kx;
    z = new_create_array(newkind, 1, 0, &n1);
    copy(z, 0, x, 0, n);
    if (newkind == atype) {
      store_array(z, n, y);
    }
    /* leave as block */
    else {
      copy1(z, n, y, 0);
      freeup(y);
    }
    freeup(x);               /* y has been freed if not stored */
  }
  else {
    z = explode(x, 1, n + 1, 0, n); /* explode frees up x */
    store_array(z, n, y);
  }
  apush(z);
}

/* the following routines implement the binary operation hitch.
 The result is a list of length one longer than the second argument with first item
  being the first argument followed by the items of the second argument.
*/

void
b_hitch()
{
  nialptr     y = apop(),
              x = apop();

  hitch(x, y);
}

void
ihitch()
{
  nialptr     x,
              y,
              z;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of hitch must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    hitch(x, y);
  }
  freeup(z);
}


void
hitch(nialptr x, nialptr y)
{
  nialptr     z;
  int         kx,
              ky,
              newkind;
  nialint     n,
              n1;

  kx = kind(x);
  ky = kind(y);
  n = tally(y);
  if (n == 0) {              /* separate case here avoids implode below */
    nial_solitary(x);
    freeup(y);
    return;
  }
  if (atomic(y)) {           /* needed to handle phrase and fault */
    pair(x, y);
    return;
  }
  if ((double) n + 1 > LARGEINT)
      exit_cover1("integer overflow in hitch", NC_WARNING);
  n1 = n + 1;
  if ((ky == kx && valence(x) == 0) || ky == atype) {
    newkind = ky;
    z = new_create_array(newkind, 1, 0, &n1);
    copy(z, 1, y, 0, n);
    if (newkind == atype) {
      store_array(z, 0, x);
    }
    /* leave as block */
    else {
      copy1(z, 0, x, 0);
      freeup(x);
    }
    freeup(y);               /* x is stored or alread freed */
  }
  else {
    z = explode(y, 1, n + 1, 1, n);
    store_array(z, 0, x);
  }
  apush(z);
}


/* routine to check that the argument is a valid shape */

static int
validshape(nialptr x)
{
  if (x == Null)
    return (true);
  if (kind(x) != inttype)
    return (false);
  {
    nialint         i;
    nialint    *ptrx = pfirstint(x);  /* safe */

    for (i = 0; i < tally(x); i++) {
      nialint     v = *ptrx++;

      if (v < 0)
        return (false);
    }
  }
  return (true);
}

/* routine to implement Nial primitive reshape.
  The usage: x reshape y produces an array of with shape given by x using the items of y cyclically.
  If x is not a list then it is treated as one. y can be of any shape.
  x must be a valid shape after being listed otherwise it returns the fault ?shape.
  IF y is empty and x implies a nonempty result then the fault ?fill is used as the 
  items of the result.
 
 
*/

void
b_reshape()
{
  nialptr     y = apop(),
              x = apop();

  if (!validshape(x)) {
    apush(makefault("?shape"));
    freeup(x);
    freeup(y);               /* in case they are split from a pair of ints */
    return;
  }
  reshape(x, y);
}

void
ireshape()
{
  nialptr     z;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of reshape must be a pair"));
  }
  else {
    nialptr     x,
                y;

    splitfb(z, &x, &y);
    if (!validshape(x)) {
      apush(makefault("?shape"));
      freeup(x);
      freeup(y);             /* in case they are split from a pair of ints */
      freeup(z);
      return;
    }
    reshape(x, y);
  }
  freeup(z);
}

/* internal version of reshape. x is known to be a valid shape */

void
reshape(nialptr x, nialptr y)
{
  int         ky = kind(y);
  nialint     i,
              ty,
              tx;

  if (ky == phrasetype || ky == faulttype) {
    nial_solitary(y);
    y = apop();              /* make y a list to simplify code below */
    ky = atype;
  }
  tx = tally(x);
  ty = tally(y);
  if (tx == 0) {             /* making a single */
    if (ty == 0)
    {           /* use a fill */
      apush(makefault("?fill"));
      freeup(x);
      freeup(y);
    }
    else {                   /* reshaping with an empty is single first y */
      apush(y);
      ifirst();
      isingle();
    }
  }
  else {
    nialint     cnt,
                j = tally(y),
                k;
    nialptr     z;
    int         kz;

    if (!prodints(pfirstint(x), tx, &cnt))
        exit_cover1("integer overflow in reshape", NC_WARNING);
    if (j == 0 && cnt != 0) {/* must fill with an item */
      nialptr f;
      f = makefault("?fill"); /* fill with a fault */
      freeup(y);
      nial_solitary(f);
      y = apop();
      ky = kind(y);
      j = 1;
    }
    kz = ky;
    if (cnt == 0)
      kz = atype;
    z = new_create_array(kz, tx, 0, pfirstint(x));
    i = 0;
    while (i < cnt) {
      k = (j < cnt - i ? j : cnt - i);
      copy(z, i, y, 0, k);   /* copy in chunks the size of y */
      i += k;
    }
    if (homotest(z))
      z = implode(z);        /* in case an atype is shortened */
    apush(z);
    freeup(x);
    freeup(y);
  }
}


/* implements the Nial primitive content recursively.
   The result is a list of the atomic leaves of the array viewed 
   as a tree found in depth first order.
*/

void
icontent()
{
  if (simple(top))
    ilist();
  else {
    int_each(icontent, apop());
    ilink();
  }
}


/* implements the Nial primitive single.
  The result is a one-item list with the argument as its item.
*/

void
isingle()
{
  if (!atomic(top)) {
    nialint     dummy;
    nialptr     x = apop(),
                z = new_create_array(atype, 0, 0, &dummy);

    store_array(z, 0, x);
    apush(z);
  }
  /* else single of an atom is the atom iteself */
}

/* implements the Nial primitive list.
 The result is the list of items of the argument.
*/

void
ilist()
{
  if (valence(top) != 1) {
    nialptr     x = apop();

    if (atomic(x))
      nial_solitary(x);
    else {
      nialint     tx = tally(x);
      nialptr     z = new_create_array(kind(x), 1, 0, &tx);
      copy(z, 0, x, 0, tx);
      if (is_sorted(x))
        set_sorted(z, true);
      apush(z);
      freeup(x);
    }
  }
  /* else list of a list is the list itself */
}

/* implements the Nial primitive pair.
   The result is a two-item array with the left argument as the
   first item and the right argument as the second item.
*/

void
ipair()
{
  if (tally(top) != 2 || valence(top) != 1)
    reshape(createint(2), apop());
  /* else pair of a pair is the pair itself and is on the stack */
}

/* routine to construct a pair from its components */

void
pair(nialptr x, nialptr y)
{
  nialptr     z;
  nialint     two = 2;
  int         kx = kind(x);

  if (kx == kind(y) && homotype(kx) && atomic(x) && atomic(y)) {
    z = new_create_array(kx, 1, 0, &two);
    copy1(z, 0, x, 0);
    copy1(z, 1, y, 0);
    freeup(x);
    freeup(y);
  }
  else {
    z = new_create_array(atype, 1, 0, &two);
    store_array(z, 0, x);
    store_array(z, 1, y);
  }
  apush(z);
}


/* routines to implement the Nial primitive predicates that
   test the various types.  */

void
iisboolean()
{
  nialptr     x = apop();

  apush(atomic(x) && kind(x) == booltype ? True_val : False_val);
  freeup(x);
}

void
iisinteger()
{
  nialptr     x = apop();

  apush(atomic(x) && kind(x) == inttype ? True_val : False_val);
  freeup(x);
}

void
iisreal()
{
  nialptr     x = apop();

  apush(atomic(x) && kind(x) == realtype ? True_val : False_val);
  freeup(x);
}

void
iischar()
{
  nialptr     x = apop();

  apush(atomic(x) && kind(x) == chartype ? True_val : False_val);
  freeup(x);
}

void
iisphrase()
{
  nialptr     x = apop();

  apush(kind(x) == phrasetype ? True_val : False_val);
  freeup(x);
}

void
iisfault()
{
  nialptr     x = apop();

  apush(kind(x) == faulttype ? True_val : False_val);
  freeup(x);
}

void
iisstring()
{ nialptr     x = apop();
  apush(isstring(x) ? True_val : False_val);
  freeup(x);
}

void
inumeric()
{ nialptr     x = apop();
  apush(numeric(kind(x)) && valence(x)==0 ? True_val : False_val);
  freeup(x);
}

void
iallbools()
{ nialptr     x = apop();
  apush(kind(x) == booltype ? True_val : False_val);
  freeup(x);
}

void
iallints()
{ nialptr     x = apop();
  apush(kind(x) == inttype ? True_val : False_val);
  freeup(x);
}

void
iallreals()
{ nialptr     x = apop();
  apush(kind(x) == realtype ? True_val : False_val);
  freeup(x);
}

void
iallchars()
{ nialptr     x = apop();
  apush(kind(x) == chartype ? True_val : False_val);
  freeup(x);
}

/* this test is true if the argument is a numeric homogeneous array. */

void
iallnumeric()
{ nialptr     x = apop();
  apush(numeric(kind(x)) ? True_val : False_val);
  freeup(x);
}

/* routine to split a pair into its components.
   This is always called with z having tally 2.
   Note that it can create temporaries from a homogeneous
   z, which are not freed when z is freed */

void
splitfb(nialptr z, nialptr * x, nialptr * y)
{
  *x = fetchasarray(z, 0);
  *y = fetchasarray(z, 1);
}


/* implements Nial primitive tally.
   The result is the number of items of the array.
 */

void
itally()
{
  nialptr     x = apop();

  apush(createint(tally(x)));
  freeup(x);
}

/* implements the Nial primitive unequal */

void
iunequal()
{
  iequal();
  inot();
}

/* implements the Nial primitive link.
   The result is the list of the items of the items of the argument.
   The result is empty if it is the Null.
*/


void
ilink()
{
  if (kind(top) != atype)
    ilist();
  else {
    nialptr     x = apop(),
               *ptrx = pfirstitem(x); /* safe */
    nialint     t = tally(x),
                i,
                tz;
    int         k = -1;      /* no kind indicator yet */
    int         ovfl;
    nialptr     zt;          /* to holld each tally result  */

    for (i = 0; i < t; i++) {/* for each item in x */
      nialptr     xi = *ptrx++;
      int         ki = kind(xi);

      if (ki != k) {         /* if not same kind */
        if (k > 0)           /* already initialized and no mismatch yet */
          k = -2;            /* mismatch */
        else if (k == -1)
          k = ki;            /* initialize kind */
      }
    }
    apush(x);                /* to protect x */
    int_each(itally, x);
    zt = apop();
    ovfl = sumints(pfirstint(zt), t, &tz);
    apop();                  /* unprotect x */
    freeup(zt);              /* no longer needed */
    if (ovfl)
        exit_cover1("integer overflow in link", NC_WARNING);
    if (tz == 0)             /* if empty case */
    { apush(Null);
      freeup(x);
    }
    else {
      nialptr     z,
                  xi;
      nialint     oi,
                  ti;

      if (homotype(k)) {
        z = new_create_array(k, 1, 0, &tz); /* make output array */
        oi = 0;
        for (i = 0; i < t; i++) {
          xi = fetch_array(x, i);
          ti = tally(xi);
          copy(z, oi, xi, 0, ti);
          oi += ti;
        }
      }
      else {
        z = new_create_array(atype, 1, 0, &tz); /* non homo result */
        oi = 0;
        for (i = 0; i < t; i++) {
          xi = fetch_array(x, i);
          ti = tally(xi);
          if (atomic(xi)) {
            store_array(z, oi, xi);
            oi++;
          }
          else {
            if (homotype(kind(xi))) {
              nialint     j;

              for (j = 0; j < ti; j++)
                store_array(z, oi++, fetchasarray(xi, j));
            }
            else {
              copy(z, oi, xi, 0, ti);
              oi += ti;
            }
          }
#ifdef USER_BREAK_FLAG
          checksignal(NC_CS_NORMAL);
#endif
        }
        if (homotest(z))     /* needed in case empties were eliminated */
          z = implode(z);
      }
      apush(z);
      freeup(x);
    }
  }
}

/* the next two routines implement the operation gage.
   It converts any array to a shape by taking its content and replacing any
   items that are not nonnegative integers by zero.
*/


void
igage()
{ nialptr x = apop();
  apush(gage(x));
}


static nialptr 
gage(nialptr x)
{ 
  nialint tx,v,i,*topx;
  int ok;
  nialptr z;
  tx = tally(x);
  if (kind(x)==inttype && tx == 1)
  { if (fetch_int(x,0)<=0) 
    { freeup(x); 
      return Zero; 
    } 
    else 
    if (atomic(x))
      return x;
    else
    { z = createint(fetch_int(x,0));
      freeup(x);
      return z;
    }
  }
  if (kind(x)==inttype && tx!=1)
  { ok = true;
    topx = pfirstint(x);
    for (i=0;i<tx;i++)
      if (*topx++ < 0) 
        ok = false;
    if (ok)
    { if (valence(x)>1)
      { apush(x);
        ilist();
        x = apop();
      }
      return x;
    }
  }
  /* need to flatten x and/or replace non ordinals with 0 */
  apush(x);
  icontent();
  if (tally(top)==1)
  { ifirst();
    isingle();
  }
  x = apop();
  if (atomic(x) && kind(x)==inttype)
  { if (intval(x)>=0)
      return x;
    else
    { freeup(x);
      return Zero;
    }
  }
  tx = tally(x);
  v = valence(x);
  z = new_create_array(tx==0?atype:inttype,v,0,shpptr(x,v));
  for (i=0;i<tx;i++)
  { nialptr it = fetchasarray(x,i);
    if (kind(it)==inttype && intval(it) >=0)
    { if (tally(x)==0)
        { store_array(z,i,Zero); }
      else
        store_int(z,i,intval(it));
    }
    else
    { if (tally(x)==0)
        { store_array(z,i,Zero); }
      else
      store_int(z,i,0);
    }
    if (it != x)
      freeup(it);
  }
  freeup(x);
  return z;
}


/* implements Nial primitive cart. 
   The result is the Cartesian product of the items of the argument
   creating an array whose shape is the link of the shapes of the items.
   Algorithm due to Carl McCrosky.
   The basic identity is that
   j pick (i pick cart A) = (i pick j') pick (j pick A)
   where j is an address in the result and j' is j grouped according 
   to the valence of the items of A. 
   The algorithm proceeds by constructing the ith item of cart A
   by walking through its index set and selecting the jth item from
   the appropriate place in the jth item of A. Carl noticed that this
   could be done by using a "fake shape" made up from the tallys of the
   items of A in an index-to-address computation applied to the ith
   index of A. The items of the resulting address are the indicies in
   the lists of items of the items of A needed to do the proper
   selection. Carl's algorithm is used for the non-boundary cases.
   If the argument is empty then the result is the single of the argument.
*/

void
icart()
{
  nialptr     arg,
              itm,
              result,
              valu,
              sc,            /* shape of cart */
              fs,            /* fetch shape */
              si,            /* shape of item */
              fa;            /* fetch address */

  nialint     i,
              j,
              indx,
              ti,            /* tally of item */
              tc;            /* tally of cart */

  if (atomic(top))
    return;                  /* atomic arg, result is arg */
  if (tally(top) == 0) {     /* empty arg */
    /* the result is single of the argument */
    isingle();
    return;
  }
  if (homotype(kind(top))) { /* simple arg, result is single of arg */
    isingle();
    return;
  }
  if (valence(top) == 0) {   /* arg is a single */
    nialptr     arg = apop(),
                it = fetch_array(arg, 0);

    int_each(isingle, it);
    freeup(arg);
    return;
  }
  apush(top);                /* will need this four times */
  apush(top);
  apush(top);
  int_each(ishape, apop());  /* uses the first copy of arg */
  ilink();
  sc = apop();               /* gives the shape of the result */

  if (!prodints(pfirstint(sc), tally(sc), &tc))
      exit_cover1("integer overflow in cart", NC_WARNING);
  int_each(itally, apop());  /* uses the second copy */
  ilink();
  fs = apop();               /* gives the tallies of the result */

  ishape();                  /* uses the third copy of the arg */
  si = apop();               /* shape of the argument */
  arg = apop();              /* get the final copy of arg  */
  ti = tally(arg);
  if (tc>0)
  {
    result = new_create_array(atype, tally(sc), 0, pfirstint(sc));
    for (i = 0; i < tc; i++) {  /* nonempty case */
      itm = new_create_array(atype, tally(si), 0, pfirstint(si));
      fa = ToAddress(i, pfirstint(fs), tally(fs));
      {
        for (j = 0; j < ti; j++) {  /* item will be nonempty. empty case done
                                     * above */
          indx = fetch_int(fa, j);
          valu = fetchasarray(fetch_array(arg, j), indx);
          store_array(itm, j, valu);
        }
        if (homotest(itm))
          { itm = implode(itm); } /* item may be homo */
      }
      store_array(result, i, itm);
      freeup(fa);
#ifdef USER_BREAK_FLAG
      checksignal(NC_CS_NORMAL);
#endif
    }
    apush(result);
    freeup(arg);
  }
  else
  { result = new_create_array(atype, tally(sc), 0, pfirstint(sc));
    freeup(arg);
    apush(result);
  }
  freeup(sc);
  freeup(si);
  freeup(fs);
}

void
ivoid()
{ nialptr x = apop();
  apush(Null);
  freeup(x);
}

/* implements the Nial primitive tell.
   The argument is expected to be an integer or a shape.
   If it is an integer N the result is the list of integers from 0
   to N-1. If the argument is a shape then the result
   is the array of addresses for that shape arranged in an
   array of the given shape. Otherwise the result is the fault ?shape.
 
*/

void
itell()
{
  nialptr     x = apop();
  int         kx = kind(x);

  if (!validshape(x))
  {
    apush(makefault("?shape"));
    freeup(x);
    return;
  }
  if (atomic(x))
  { nialint n;
    n = (kx == inttype ? intval(x) : 0);  /* to distinguish Null case */
    apush(generateints(n));
    set_sorted(top, true);
    freeup(x);
  }
  else 
  {
    int_each(itell, x);
    icart();
    set_sorted(top, true);
  }
}

/* internal routine for tell of an integer */

nialptr
generateints(nialint n)
{
  if (n == 0)
    return (Null);           /* the empty list is the result */
  else {
    nialptr     z = new_create_array(inttype, 1, 0, &n);
    nialint    *topz = pfirstint(z),  /* safe */
                i;

    for (i = 0; i < n; i++)
      *topz++ = i;
    return (z);
  }
}


/* primitive operation grid */

void
igrid(void)
{
  ishape();
  if (tally(top)==1)
    ifirst();
  itell();
}


/* routines to implement Nial primitives take and drop.
   take and drop return a portion of the second argument 
   based on the integer values in the first argument.
   "take" uses the integers to determine the extent of each axis to include.
   Negative integers indicate taking from the upper end of the axis.
   If the integer is larger than the extent of the axis then extra items are
   used to fill the array. For a homogeneous array the typical item is used to
   fill, otherwise the fault ?fill is used.
   "drop" uses the integer values to determine how much of the extent of 
   the axis to omit. Negative integers indicate dropping from the upper end of the axis.
   If the integer is larger than the extent of the axis then the result is empty.

   The first argument of take and drop must have tally equal to the valence
   of the second argument otherwise a fault is returned. 
  */

void
b_take()
{
  nialptr     y = apop(),
              x = apop();

  takedrop(x, y, true);
}

void
itake()
{
  nialptr     z,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of take must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    takedrop(x, y, true);
  }
  freeup(z);
}

void
b_drop()
{
  nialptr     y = apop(),
              x = apop();

  takedrop(x, y, false);
}

void
idrop()
{
  nialptr     z,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of drop must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    takedrop(x, y, false);
  }
  freeup(z);
}


/* routine to implement details of the take and drop operations.
   For take: if items are all <= shape of y, then the result is defined by
       x take y = tell x choose y
   but we do the computation directly to avoid address checks in choose.
   For x with negative items, the indexes are modified to pick from
   the end. If the index is out of range due to an overtake,
   then a fill item is used. It is the typical item of the kind
   or the fault ?fill if the kind is atype.
   For drop: the corresponding take ranges are found and the code merged.
   An over drop result in an empty.
*/

static void
takedrop(nialptr x, nialptr y, int istake)
{
  nialptr     z,
              fillitem = Null;
  int         fillused = false;
  nialint     i,
              j,
              index,
              tz,
              ty,
              tx = tally(x);
  int         vy = valence(y),
              ky = kind(y);

/* moved here for gdb */
  nialint     pos,
              offset,
              limit,
              bound,
              lastdim,
              lastdimaddr;
  nialptr     starts,
              lengths,
              addr,
              strides;
  int         usefill,
              processaxes,
              dim;

  if (kind(x) != inttype && tx > 0) {
    if (istake) {
      apush(makefault("?left argument in take must be integers"));
    }
    else {
      apush(makefault("?left argument in drop must be integers"));
    }
    freeup(x);
    freeup(y);
    return;
  }
  if (vy == 0) {
    if (istake) {            /* take on a single is treated as reshape with
                              * abs A on the single */
      apush(x);
      iabs();
      pair(apop(), y);
      ireshape();
      return;
    }
    else {                   /* a drop */
      if (tx == 0) {
        apush(y);
      }                      /* Null drop on a single is the single */
      else {
        apush(Null);         /* other drops on a single is Null */
        freeup(y);
      }
      freeup(x);
      return;
    }
  }

  if (tx != vy) {
    if (istake) {
      apush(makefault("?valence error in take"));
    }
    else {
      apush(makefault("?valence error in drop"));
    }
    freeup(x);
    freeup(y);
    return;
  }

  if (tx == 0) {             /* taking with an empty shape */
    apush(y);
    ifirst();                /* single result */
    isingle();
    freeup(x);
    return;
  }


  ty = tally(y);

  if (vy == 1) {             /* handle lists case separately for speed */
    nialint     start,
                length,
                xval;

    /* set up start and length */
    xval = fetch_int(x, 0);
    if (istake) {
      if (xval >= 0) {
        start = 0;
        length = xval;
      }
      else {
#ifdef INTS32
          start = ty - nialabs(xval); /* can be < 0 */
          length = nialabs(xval);
#elif INTS64
          start = ty - nialabs(xval); /* can be < 0 */
          length = nialabs(xval);
#else
#error precision not defined
#endif
      }
    }
    else {                   /* a drop */
      if (xval >= 0) {
        start = (xval < ty ? xval : ty);
        length = (ty >= xval ? ty - xval : 0);
      }
      else {
        start = 0;
#ifdef INTS32
        length = (ty >= nialabs(xval) ? ty - nialabs(xval) : 0);
#elif INTS64
        length = (ty >= nialabs(xval) ? ty - nialabs(xval) : 0);
#else
#error precision not defined
#endif
      }
    }


    if (length == 0) {       /* empty result expected. Use reshape to
                              * compute the empty */
      apush(createint(length));
      apush(y);
      b_reshape();           /* frees y */
      freeup(x);
      return;
    }

    /* create container of shape given by length */

    z = new_create_array(ky, 1, 0, &length);
    tz = tally(z);

    /* fill in the result */
    for (i = 0; i < tz; i++) {
      index = start + i;
      if (index >= 0 && index < ty) {
        copy1(z, i, y, index);
      }
      else {
        if (!fillused) {
          if (ty > 0) {      /* fill with type of first item */
            apush(fetchasarray(y, 0));
            itype();
            fillitem = apop();
          }
          else               /* fill with faults. Container will be atype */
            fillitem = makefault("?fill");
          fillused = true;
        }
        if (kind(z) == atype) {
          store_array(z, i, fillitem);
        }
        else                 /* fillitem is an atom of the same homogeneous
                              * type as z */
          copy1(z, i, fillitem, 0);
      }
    }
  }
  else {
    /* set up starts and lengths for each dimension in the result */

    starts = new_create_array(inttype, 1, 0, &tx);
    lengths = new_create_array(inttype, 1, 0, &tx);
    for (i = 0; i < tx; i++) {
      nialint     xi = fetch_int(x, i),
                  shi = pickshape(y, i);

      if (istake) {
        if (xi >= 0) {
          store_int(starts, i, 0);
          store_int(lengths, i, xi);
        }
        else {
#ifdef INTS32
          store_int(starts, i, shi - nialabs(xi));  /* can be < 0 */
          store_int(lengths, i, nialabs(xi));
#elif INTS64
          store_int(starts, i, shi - nialabs(xi));  /* can be < 0 */
          store_int(lengths, i, nialabs(xi));
#else
#error precision not defined
#endif
        }
      }
      else {                 /* a drop */
        if (xi >= 0) {
          store_int(starts, i, (xi < shi ? xi : shi));
          store_int(lengths, i, (shi >= xi ? shi - xi : 0));
        }
        else {
          store_int(starts, i, 0);
#ifdef INTS32
          store_int(lengths, i, (shi >= nialabs(xi) ? shi - nialabs(xi) : 0));
#elif INTS64
          store_int(lengths, i, (shi >= nialabs(xi) ? shi - nialabs(xi) : 0));
#else
#error precision not defined
#endif
        }
      }
    }

    /* compute the tally of the result */
    if (!prodints(pfirstint(lengths), tx, &tz))
        exit_cover1("integer overflow in take", NC_WARNING);

    if (tz == 0) {           /* empty result expected *//* u se reshape to
                              * compute the empty */
      apush(lengths);
      apush(y);
      b_reshape();           /* frees lengths and y */
      freeup(starts);
      freeup(x);
      return;
    }

    /* create container of shape given by lengths */

    z = new_create_array(ky, vy, 0, pfirstint(lengths));
    tz = tally(z);

    /* create a container to build corresponding address in */
    addr = new_create_array(inttype, 1, 0, &tx);
    for (j = 0; j < tx; j++)
      store_int(addr, j, fetch_int(starts, j));

    /* compute the strides. Used for incrementing offset */
    strides = new_create_array(inttype, 1, 0, &tx);
    store_int(strides, tx - 1, 1);
    for (j = tx - 2; j >= 0; j--)
      store_int(strides, j, pickshape(y, j + 1) * fetch_int(strides, j + 1));

    pos = 0;                 /* cycles over the last axis of the result */

    /* intial offset is index of addr. Compute it using Horner's rule */
    offset = 0;
    for (j = 0; j < tx; j++)
      offset = offset * pickshape(y, j) + fetch_int(addr, j);

    lastdim = vy - 1;        /* last axis of y and z */
    limit = fetch_int(lengths, lastdim);  /* length of last axis of z */
    bound = pickshape(y, lastdim);  /* length of last axis of y */

    /* compute usefill flag which is set when all the entries on the last
     * axis for one sweep are fill items. */
    usefill = false;
    for (j = 0; j < lastdim; j++) {
      nialint     jthaddr = fetch_int(addr, j);

      if (jthaddr < 0 || jthaddr >= pickshape(y, j))
        usefill = true;
    }

    /* loop to walk over indices of the result selecting from y using an
     * offset based on starts and shape of y. */
    for (i = 0; i < tz; i++) {  /* compute the index for the item of y and
                                 * move it to the result */
      /* check that addr is in bounds on last axis of y */
      lastdimaddr = fetch_int(addr, lastdim);
      if (!usefill && lastdimaddr >= 0 && lastdimaddr < bound) {
        index = offset + pos;
        copy1(z, i, y, index);
      }
      else {                 /* do a fill */
        if (!fillused) {
          if (ty > 0) {      /* fill with type of first item */
            apush(fetchasarray(y, 0));
            itype();
            fillitem = apop();
          }
          else               /* fill with faults. Container will be atype */
            fillitem = makefault("?fill");
          fillused = true;
        }
        if (kind(z) == atype) {
          store_array(z, i, fillitem);
        }
        else                 /* fillitem is an atom of the same homogeneous
                              * type as z */
          copy1(z, i, fillitem, 0);
      }

      /* increment pos and last dim of addr */
      pos++;
      store_int(addr, lastdim, 1 + lastdimaddr);
      if (pos == limit) {    /* loop over final axis of z is completed. do
                              * the end of axes processing */
        pos = 0;             /* reset pos and addr to walk last axis again */
        store_int(addr, lastdim, fetch_int(starts, lastdim));
        processaxes = true;
        dim = tx - 2;        /* other axes are incremented and checked */
        while (processaxes && dim >= 0) { /* increment the address in the
                                           * current dimension */
          store_int(addr, dim, 1 + fetch_int(addr, dim));
          if (fetch_int(addr, dim) == fetch_int(starts, dim) + fetch_int(lengths, dim)) {
            store_int(addr, dim, fetch_int(starts, dim)); /* reset dim address
                                                           * position */
            /* compute the new offset */
            offset = 0;
            for (j = 0; j < tx; j++)
              offset = offset * pickshape(y, j) + fetch_int(addr, j);


            dim--;           /* stay in the while loop to set the next lower
                              * axis */
          }
          else {
            processaxes = false;
            offset += fetch_int(strides, dim);  /* increment the offset */
          }
        }
        /* recompute the usefill flag */
        usefill = false;
        for (j = 0; j < lastdim; j++) {
          nialint     jthaddr = fetch_int(addr, j);

          if (jthaddr < 0 || jthaddr >= pickshape(y, j))
            usefill = true;
        }

#ifdef USER_BREAK_FLAG
        checksignal(NC_CS_NORMAL);
#endif
      }
    }
    freeup(starts);
    freeup(lengths);
    freeup(addr);
    freeup(strides);
  }
  if (homotest(z))
    z = implode(z);          /* in case selection reduces to homo */
  apush(z);
  freeup(x);
  freeup(y);
  if (fillused)
    freeup(fillitem);
}


/* utility routine to convert an integer index into a Nial address, given a ptr to the
   shape and the valence of the array that the address is for.  
   Used in atops.c and trs.c.
*/

nialptr
ToAddress(nialint index, nialint * shp, nialint v)
{
  if (v == 0)
    return (Null);
  /* address to be built */
  {
    nialptr     z;
    nialint     shapei,
                i,
               *addr = (nialint *) malloc(sizeof(nialint) * v);

    if (!addr)
      exit_cover1("Not enough memeory to continue", NC_FATAL);

    for (i = v - 1; i >= 0; i--) {
      shapei = *(shp + i);
      if (shapei == 0)
        addr[i] = 0;
      else {
        addr[i] = index % shapei;
        index = index / shapei;
      }
    }
    z = new_create_array(inttype, 1, 0, &v);
    for (i = 0; i < v; i++)
      store_int(z, i, addr[i]);
    free(addr);
    return (z);
  }
}


/* the following routines implement the primitive operations cut and cutall. 

   The operation x cut y converts an array y into a list of items formed from 
   the items of y according to the bitstring x.  The list of items is divided 
   where true values occur in the corresponding positions in the bitstring x.  
   The items of y where the divisions occur are not included in the items of the 
   result and any empty segments are not included. 

   The operation x cutall y converts an array y into a list of items formed from 
   the items of y according to the bitstring x.  The list of items is divided where 
   true values occur in the corresponding positions in the bitstring x.  The items 
   where the divisions occur are kept as the first item of each group. 

*/

void
b_cut()
{
  nialptr     y = apop(),
              x = apop();

  cutorcutall(x, y, true);
}

void
icut()
/* implements cut directly */
{
  nialptr     x,
              y,
              z;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of cut must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    cutorcutall(x, y, true);
  }
  freeup(z);
}

/* primitive operation cutall */

void
b_cutall()
{
  nialptr     y = apop(),
              x = apop();

  cutorcutall(x, y, false);
}

void
icutall()
/* implements cutall directly */
{
  nialptr     x,
              y,
              z;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of cutall must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    cutorcutall(x, y, false);
  }
  freeup(z);
}

/* routine that implements both cut and cutall */

/* new algorithm internals to avoid use of stack */

static void
cutorcutall(nialptr x, nialptr y, int cutprim)
{
  nialint     i, ty, j, cnt;
  nialptr     starts,lengths, itm, z;
  nialint     length;
  int         ky;

  if (kind(x) != booltype && tally(x) != 0) {
    if (cutprim)
      { apush(makefault("?first arg of cut is not boolean")); }
    else
      apush(makefault("?first arg of cutall is not boolean"));
    freeup(x);
    freeup(y);
    return;
  }
  ty = tally(y);
  if (tally(x) != ty)        /* cycle left argument */
  { if (tally(x) == 0)
    { if (cutprim)
        { apush(makefault("?first arg of cut is empty")); }
      else
        { apush(makefault("?first arg of cutall is empty")); }
      freeup(x);
      freeup(y);
      return;
    }
    else
    { reshape(createint(ty), x);
      x = apop();
    }
  }
  starts = new_create_array(inttype,1,0,&ty);
  lengths = new_create_array(inttype,1,0,&ty);
  ky = kind(y);
  if (ky == phrasetype || ky == faulttype)
  { /* turn y into a list of one item */
    apush(y);
    ilist();
    y = apop();
    ky = kind(y);
  }
  j = 0;
  cnt = 0;
  for (i = 0; i < ty; i++)
  { if (fetch_bool(x, i))   /* if truth value */
    { length = i - j;
      /* make a list of items which have been kept since the last true value,
       * if any */
      if (length > 0)
      { store_int(starts,cnt,j);
        store_int(lengths,cnt,length);
        cnt++;
      }
      j = (cutprim ? i + 1 : i);
    }
#ifdef USER_BREAK_FLAG
    checksignal(NC_CS_NORMAL);
#endif
  }
  /* finish the last sublist */
  length = i - j;
  if (length > 0)
  { store_int(starts,cnt,j);
    store_int(lengths,cnt,length);
    cnt++;
  }
  z = new_create_array(atype,1,0,&cnt);
  for (i=0;i<cnt;i++)
  { length = fetch_int(lengths,i);
    itm = new_create_array(ky, 1, 0, &length);
    copy(itm, 0, y, fetch_int(starts,i),length);
    store_array(z,i,itm);
  }
  apush(z);
  freeup(starts);
  freeup(lengths);
  freeup(x);
  freeup(y);
}

/* The next 3 routines implement the operation sublist

   The operation  x sublist y returns a list of items of y chosen according 
   to the list of booleans given in x, selecting those items of y where the 
   corresponding item of x is true.  If the tally of x is not the same as the 
   tally of y, x is coerced to have the same tally as y using reshape.  If y is 
   not a list, the result is the same as applying sublist to the list of y.  
   The tally of the result is the sum of x after it has been reshaped, if necessary. 
   The result is a fault if x is not boolean or if x is empty.
 
*/

void
b_sublist()
{
  nialptr     y = apop(),
              x = apop();

  sublist(x, y);
}

void
isublist()
{
  nialptr     x,
              y,
              z;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of sublist must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    sublist(x, y);
  }
  freeup(z);
}

void
sublist(nialptr x, nialptr y)
{
  nialptr     z;
  nialint     i,
              j,
              n,
              ty;

  if (kind(x) != booltype && tally(x) != 0) {
    apush(makefault("?first arg of sublist not boolean"));
    freeup(x);
    freeup(y);
    return;
  }
  ty = tally(y);
  if (tally(x) != ty)
  {
    if (tally(x) == 0)
    { apush(makefault("?first arg of sublist is empty"));
      freeup(x);
      freeup(y);
      return;
    }
    else
    { reshape(createint(ty), x);
      x = apop();
    }
  }
  if (atomic(y))             /* needed for phrasetype and faulttype */
  { apush(y);
    ilist();
    y = apop();
  }
  n = 0;                     /* count number of true items in x */
  for (i = 0; i < ty; i++)
    if (fetch_bool(x, i))
      n++;
  if (n > 0) {               /* create container and fill it */
    int         ky = kind(y);
    z = new_create_array(ky, 1, 0, &n);
    j = 0;
    for (i = 0; i < ty; i++) {
      if (fetch_bool(x, i)) {
        copy1(z, j, y, i);
        j++;
      }
    }
    if (homotest(z))
      z = implode(z);        /* in case all non-atomics eliminated */
    if (is_sorted(y))
      set_sorted(z, true);
    apush(z);
  }
  else
    apush(Null);
  freeup(x);
  freeup(y);
}

/* The next 2 routine implement the primitive predicate simple.
   The result is true for all empty arrays and for
   nonempty arrays with items that are all atoms.
 */

void
isimple()
{
  nialptr     x;

  x = apop();
  if (simple(x)) {
    apush(True_val);
  }
  else
    apush(False_val);
  freeup(x);
}

int
simple(nialptr x)
{
  nialint     i,
              t;
  int         simp = true;
  nialptr    *ptrx,
              xi;

  if (kind(x) != atype)
    return (true);
  t = tally(x);
  if (t == 0)
    return (true);
  i = 0;
  ptrx = pfirstitem(x);      /* safe */
  while (simp && i++ < t) {
    xi = *ptrx++;            /* do not combine these; atomic uses its arg
                              * twice */
    simp = atomic(xi);
  }
  return (simp);
}

/* The next set of routines implements the operation pack, which plays a crucial
   role in the definition of the pervasive operations.

   The operation pack interchanges the top two levels of an
   equishaped array. If the array is not equishaped then it is made
   equishaped if it has conforming items, i.e. all items having more
   than one item are the same shape. In that case, the unitary items
   are extended to the shape of the other items.

   If A is simple (including empty), pack A = single A.
      otherwise  I pick (J pick pack A') = J pick (I pick A')
                 where A' is the equishaped extension of A.

   If A is not equishaped and does not have conforming items the result
   is fault ?conform.

  The operation flip is and alternative name for pack.
*/


void
ipack()
{
  if (atomic(top))
    return;
  if (simple(top))
    isingle();               /* do this so that sum a = EACH sum pack a holds
                              * for empty a */
  else
    pack(apop());
}

static void
pack(nialptr x)
{
  nialptr     x0;
  nialint     i,
              tallyx,
             *shp = NULL;
  int         vf = 0,
              sameshapes;

  tallyx = tally(x);

  sameshapes = true;         /* if all same shape then quit early */
  x0 = fetch_array(x, 0);
  i = 1;
  while (sameshapes && (i < tallyx)) {
    sameshapes = equalshape(x0, fetch_array(x, i));
    i++;
  }
  if (sameshapes) {          /* do the replication work */
    vf = valence(x0);
    shp = shpptr(x0, vf);
  }
  else {
    int         shapefound = false,
                pos = -1,
                maxv = 0,    /* do not set this to -1 because valence is
                              * unsigned and the comparison valence(x0) >
                              * maxv will fail. */
                maxvpos = 0;

    for (i = 0; i < tallyx; i++) {
      x0 = fetch_array(x, i);
      if (tally(x0) != 1) {
        if (!shapefound) {
          shapefound = true;
          shp = shpptr(x0, valence(x0));
          pos = i;
          vf = valence(x0);
        }
        else {
          if (!equalshape(x0, fetch_array(x, pos)))
            /* if all the non-singular shapes are not the same return the
             * fault ?conform if in trim or pack */
          {
            apush(makefault("?conform"));
            freeup(x);
            return;
          }
        }
      }
      else {
        if (!shapefound && valence(x0) > maxv) {
          maxv = valence(x0);
          maxvpos = i;
        }
      }
    }
    if (!shapefound) {       /* all items have tally 1 */
      shp = shpptr(fetch_array(x, maxvpos), maxv);
      vf = maxv;
    }
  }
  /* now do the flip work with arrays of the same shape */
  {
    nialptr     z,
                nit,
                it,
                it1;
    nialint     i,
                j,
                limit,
                limitf,
                tx;
    int         vx;

    tx = tally(x);
    vx = valence(x);
    prodints(shp, vf, &limitf); /* no check needed on product */
    limit = tx;
    z = new_create_array(atype, vf, 0, shp);  /* Safe: no heap creations
                                               * between assigning shp and
                                               * here. */
    for (i = 0; i < limitf; i++) {
      nit = new_create_array(atype, vx, 0, shpptr(x, vx));
      for (j = 0; j < limit; j++) {
        it = fetch_array(x, j);
        if (tally(it) == 1)
          it1 = fetchasarray(it, 0);
        else
          it1 = fetchasarray(it, i);
        if (vx == 0 && atomic(it1)) { /* item is to be an atom. Thus freeup
                                       * the container here before it gets
                                       * given a possibly temporary item.
                                       * This is safe to do since the loop
                                       * only goes around once in this case. */
          freeup(nit);
          nit = it1;         /* item is the atom found */
        }
        else
          store_array(nit, j, it1);
      }
      if (homotest(nit))
        nit = implode(nit);
      store_array(z, i, nit);
#ifdef USER_BREAK_FLAG
      checksignal(NC_CS_NORMAL);
#endif
    }
    if (homotest(z))
      z = implode(z);
    apush(z);
    freeup(x);
  }
}


/* The next set of routines implement the primitive operations 
   seek, find, in and findall.
   The first three are related by the equation
       A seek B = [A in B,A find B]  
   
   A in B returns true if A is an item of B false otherwise,
   A find B returns the address in B when A is found, or shape B if not.
   A findall B returns a list of addresses for the locations of A as an
   item of B.

   In cases where A is a sorted array there are fast versions for both seek
   and findall using a binary search algorithm.
*/

void
b_seek()
{
  nialptr     y = apop(),
              x = apop();

  nseek(x, y);
}

void
iseek()
{
  nialptr     a,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  a = apop();
  if (tally(a) != 2) {
    apush(makefault("?argument to seek must be a pair"));
  }
  else {
    splitfb(a, &x, &y);
    nseek(x, y);
  }
  freeup(a);
}

void
b_find()
{
  nialptr     y = apop(),
              x = apop();

  nseek(x, y);
  isecond();
}

void
ifind()
{
  nialptr     a,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  a = apop();
  if (tally(a) != 2) {
    apush(makefault("?argument to find must be a pair"));
  }
  else {
    splitfb(a, &x, &y);
    nseek(x, y);
    isecond();
  }
  freeup(a);
}

void
b_in()
{
  nialptr     y = apop(),
              x = apop();

  nseek(x, y);
  ifirst();
}

void
iin()
{
  nialptr     a,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  a = apop();
  if (tally(a) != 2) {
    apush(makefault("?argument to in must be a pair"));
  }
  else {
    splitfb(a, &x, &y);
    nseek(x, y);
    ifirst();
  }
  freeup(a);
}

void
b_findall()
{
  nialptr     y = apop(),
              x = apop();

  findall(x, y);
}

void
ifindall()
{
  nialptr     a,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  a = apop();
  if (tally(a) != 2) {
    apush(makefault("?argument to findall must be a pair"));
  }
  else {
    splitfb(a, &x, &y);
    findall(x, y);
  }
  freeup(a);
}

/* routine is called nseek to avoid conflict with library routine */

static void
nseek(nialptr x, nialptr y)
{
  nialint     ty,
              i = 0;
  int         res = false,
              v = valence(y);

  if (atomic(y)) {
    if (equal(x, y))
      pair(True_val, Null);
    else
      pair(False_val, Null);
    return;
  }
  ty = tally(y);
  
  if (is_sorted(y) && kind(x) != booltype && ty != 0)
    /* use the binary search algorithm if the types match */
  {
    if (atomic(x))
      switch (kind(x)) {
        case inttype:
            if (kind(y) == inttype)
              sfindallint(x, y, true);
            else
              goto merge_nseek;
            break;
        case realtype:
            if (kind(y) == realtype)
              sfindallreal(x, y, true);
            else
              goto merge_nseek;
            break;
        case chartype:
            goto merge_nseek;
        case phrasetype:
        case faulttype:
            if (kind(y) == atype)
              sfindallatype(x, y, true);
            else
              goto merge_nseek;
      }
    else {
      if (kind(y) == atype)
        sfindallatype(x, y, true);
      else
        goto merge_nseek;
    }
    return;                  /* the sfindall routines handle the result
                              * construction */
  }
merge_nseek:
  if (kind(x) == kind(y) && atomic(x)) {  /* do a homotype search */
    switch (kind(x)) {
      case realtype:
          {
            double      r = realval(x),
                       *yptr = pfirstreal(y); /* safe */

            i = 0;
            res = false;
            while (!res && i < ty) {
              res = r == *yptr++;
              i++;
            }
          }
          break;
      case inttype:
          {
            nialint     xv = intval(x),
                       *yptr = pfirstint(y);  /* safe */

            i = 0;
            res = false;
            while (!res && i < ty) {
              res = xv == *yptr++;
              i++;
            }
          }
          break;
      case chartype:
          {
            char        xv = charval(x),
                       *yptr = pfirstchar(y); /* safe */

            i = 0;
            res = false;
            while (!res && i < ty) {
              res = xv == *yptr++;
              i++;
            }
          }
          break;
      case booltype:
          {
            int         xv = boolval(x);

            i = 0;
            res = false;
            while (!res && i < ty) {
              res = xv == fetch_bool(y, i);
              i++;
            }
          }
          break;
    }
    freeup(x);
  }
  else {
    apush(x);                /* to protect it for multiple calls of equal */
    i = 0;
    res = false;
    while (!res && i < ty)
      res = equal(x, fetchasarray(y, i++));
    freeup(apop());          /* free protected x */
  }
  apush(createbool(res));
  if (res) {
    if (v == 1) {
      apush(createint(i - 1));
    }
    else {
      apush(ToAddress(i - 1, shpptr(y, v), v));
    }
    freeup(y);
  }
  else {
    if (v == 1) {
      apush(createint(ty));
      freeup(y);
    }
    else {
      apush(y);
      ishape();
    }
  }
  mklist(2);
}

void
findall(nialptr x, nialptr y)
{
  nialint     ty,
              i,
              finds;
  int         v = valence(y);
  nialptr     z,
              res;

  if (atomic(y)) {           /* only address of an atom is Null */
    if (equal(x, y)) {
      nial_solitary(Null);   /* list of one Null */
    }
    else {
      apush(Null);           /* result is Null */
    }
    return;
  }
  ty = tally(y);
  /* if ((is_sorted(y) || check_sorted(y)) && kind(x) != booltype && ty != 0) */
  if (is_sorted(y) && kind(x) != booltype && ty != 0) { /* try to use the binary
                                                         * search algorithm */
    if (atomic(x))
      switch (kind(x)) {
        case inttype:
            if (kind(y) == inttype)
              sfindallint(x, y, false);
            else
              goto findall_merge;
            break;
        case realtype:
            if (kind(y) == realtype)
              sfindallreal(x, y, false);
            else
              goto findall_merge;
            break;
        case chartype:
            goto findall_merge;
        case phrasetype:
        case faulttype:
            if (kind(y) == atype)
              sfindallatype(x, y, false);
            else
              goto findall_merge;
            break;
      }
    else {
      if (kind(y) == atype)
        sfindallatype(x, y, false);
      else
        goto findall_merge;
    }
    return;                  /* sfindall routines construct the result */
  }
findall_merge:
  /* create result container at maximum size */
  res = new_create_array(v == 1 ? inttype : atype, 1, 0, &ty);
  finds = 0;
  apush(x);                  /* protect x */
  for (i = 0; i < ty; i++) {
    if (equal(x, fetchasarray(y, i))) {
      if (v == 1)
        store_int(res, finds, i);
      else
        store_array(res, finds, ToAddress(i, shpptr(y, v), v));
      finds++;
    }
  }
  if (finds > 0) {           /* copy the used part of res into z */
    if (v == 1) {
      z = new_create_array(inttype, 1, 0, &finds);
      for (i = 0; i < finds; i++)
        store_int(z, i, fetch_int(res, i));
    }
    else {
      z = new_create_array(atype, 1, 0, &finds);
      for (i = 0; i < finds; i++)
        store_array(z, i, fetch_array(res, i));
    }
  }
  else {                     /* result is Null */
    z = Null;
  }
  freeup(y);
  freeup(apop());            /* free protected x */
  apush(z);
  freeup(res);
}


/* binary search findall on a sorted array */

/* The sfindall routines - int, real, atype - use the same algorithm to
   implement a binary search for the first and last items of y that are
   equal to x. If firstonly is true only the first item is sought and
   the algorithm returns the "seek" result of a boolean and an address.
   Otherwise it returns the "findall" result.

   The algorithm does a binary search to find the first item equal to
   the target value, using low and midlow as the boundaries of the search.
   If requested it then does a binary search to find the last item equal to the
   target value using midhigh and high as the boundaries. In order to
   make the latter search symmetric with the former, the highprobe is biased
   high.
*/

static void
sfindallint(nialptr x, nialptr y, int firstonly)
{
  nialint     ty = tally(y);
  int         v = valence(y);
  nialint     target,
              low,
              high,
              midlow,
              midhigh;
  int         lowdone,
              highdone;

  target = intval(x);
  low = 0;
  high = ty - 1;
  midlow = ty - 1;
  lowdone = false;
  highdone = firstonly;
  while (!lowdone) {
    /* ensure the target is in the intervals */
    if (fetch_int(y, low) > target) {
      lowdone = true;
      highdone = true;
      low = high + 1;        /* to force Null result */
    }
    else
      /* reduce the interval at the low end */
    {
      nialint     lowprobe = (low + midlow) / 2;
      nialint     lowprobeval = fetch_int(y, lowprobe);

      if (lowprobeval == target) {
        midlow = lowprobe;
        lowdone = low == lowprobe;  /* test if low boundary found */
      }
      else if (lowprobeval < target) {  /* increase low */
        low = lowprobe + 1;
        lowdone = low > ty - 1;
      }
      else {                 /* lowprobeval > target ; decrease midlow */
        midlow = lowprobe - 1;
        lowdone = midlow < low;
      }
    }
  }
  high = ty - 1;
  midhigh = low;
  while (!highdone) {
    /* ensure the target is in the intervals */
    if (fetch_int(y, high) < target) {
      highdone = true;
      low = high + 1;        /* to force Null result */
    }
    else
      /* reduce the interval at the high end */
    {
      nialint     highprobe = (high + midhigh + 1) / 2;
      nialint     highprobeval = fetch_int(y, highprobe);

      if (highprobeval == target) {
        midhigh = highprobe;
        highdone = high == highprobe;
      }
      else if (highprobeval > target) {
        high = highprobe - 1;
        highdone = high < 0;
      }
      else
        /* highprobe < target */
      {
        midhigh = highprobe + 1;
        highdone = midhigh > high;
      }
    }
  }
  if (firstonly) {           /* set up a "seek" result */
    apush(createbool(low <= high));
    if (low <= high) {
      if (v == 1) {
        apush(createint(low));
      }
      else {
        apush(ToAddress(low, shpptr(y, v), v));
      }
    }
    else {
      if (v == 1) {
        apush(createint(ty));
        freeup(y);
      }
      else {
        apush(y);
        ishape();
      }
    }
    mklist(2);
  }
  else {                     /* set up a "findall" result */
    if (low > high) {
      apush(Null);
    }
    else {
      nialint     i,
                  size = high - low + 1;
      nialptr     z;

      if (v == 1) {
        z = new_create_array(inttype, 1, 0, &size);
        for (i = 0; i < size; i++)
          store_int(z, i, low + i);
        apush(z);
      }
      else {
        z = new_create_array(atype, 1, 0, &size);
        for (i = 0; i < size; i++)
          store_array(z, i, ToAddress(low + i, shpptr(y, v), v));
        apush(z);
      }
    }
  }
  freeup(x);
  freeup(y);
}

static void
sfindallreal(nialptr x, nialptr y, int firstonly)
{
  nialint     ty = tally(y);
  int         v = valence(y);
  nialint     low,
              high,
              midlow,
              midhigh;
  double      target;
  int         lowdone,
              highdone;

  target = realval(x);
  low = 0;
  high = ty - 1;
  midlow = ty - 1;
  lowdone = false;
  highdone = firstonly;
  while (!lowdone) {
    /* ensure the target is in the intervals */
    if (fetch_real(y, low) > target) {
      lowdone = true;
      highdone = true;
      low = high + 1;        /* to force Null result */
    }
    else
      /* reduce the interval at the low end */
    {
      nialint     lowprobe = (low + midlow) / 2;
      double      lowprobeval = fetch_real(y, lowprobe);

      if (lowprobeval == target) {
        midlow = lowprobe;
        lowdone = low == lowprobe;  /* test if low boundary found */
      }
      else if (lowprobeval < target) {  /* increase low */
        low = lowprobe + 1;
        lowdone = low > ty - 1;
      }
      else
        /* lowprobeval > target ; decrease midlow */
      {
        midlow = lowprobe - 1;
        lowdone = midlow < low;
      }
    }
  }
  high = ty - 1;
  midhigh = low;
  while (!highdone) {

     /* ensure the target is in the intervals */ if (fetch_real(y, high) < target) {
      highdone = true;
      low = high + 1;        /* to force Null result */
    }
    else
      /* reduce the interval at the high end */
    {
      nialint     highprobe = (high + midhigh + 1) / 2;
      double      highprobeval = fetch_real(y, highprobe);

      if (highprobeval == target) {
        midhigh = highprobe;
        highdone = high == highprobe;
      }
      else if (highprobeval > target) {
        high = highprobe - 1;
        highdone = high < 0;
      }
      else
        /* highprobe < target */
      {
        midhigh = highprobe + 1;
        highdone = midhigh > high;
      }
    }
  }

  if (firstonly) {           /* set up a "seek" result */
    apush(createbool(low <= high));
    if (low <= high) {
      if (v == 1) {
        apush(createint(low));
      }
      else {
        apush(ToAddress(low, shpptr(y, v), v));
      }
    }
    else {
      if (v == 1) {
        apush(createint(ty));
        freeup(y);
      }
      else {
        apush(y);
        ishape();
      }
    }
    mklist(2);
  }
  else {                     /* set up a "findall" result */
    if (low > high) {
      apush(Null);
    }
    else {
      nialint     i,
                  size = high - low + 1;
      nialptr     z;

      if (v == 1) {
        z = new_create_array(inttype, 1, 0, &size);
        for (i = 0; i < size; i++)
          store_int(z, i, low + i);
        apush(z);
      }
      else {
        z = new_create_array(atype, 1, 0, &size);
        for (i = 0; i < size; i++)
          store_array(z, i, ToAddress(low + i, shpptr(y, v), v));
        apush(z);
      }
    }
  }
  freeup(x);
  freeup(y);
}

static void
sfindallatype(nialptr x, nialptr y, int firstonly)
{
  nialint     ty = tally(y);
  int         v = valence(y);
  nialint     low,
              high,
              midlow,
              midhigh;
  nialptr     target;
  int         lowdone,
              highdone;

  target = x;
  apush(x);                  /* protect x in uses of equal and up as target */
  low = 0;
  high = ty - 1;
  midlow = ty - 1;
  lowdone = false;
  highdone = firstonly;
  while (!lowdone) {
    /* ensure the target is in the intervals */
    if (up(target, fetch_array(y, low)) &&
        !equal(target, fetch_array(y, low))) {
      lowdone = true;
      highdone = true;
      low = high + 1;        /* to force Null result */
    }
    else
      /* reduce the interval at the low end */
    {
      nialint     lowprobe = (low + midlow) / 2;
      nialptr     lowprobeval = fetch_array(y, lowprobe);

      if (equal(lowprobeval, target)) {
        midlow = lowprobe;
        lowdone = low == lowprobe;  /* test if low boundary found */
      }
      else if (up(lowprobeval, target)) { /* increase low */
        low = lowprobe + 1;
        lowdone = low > ty - 1;
      }
      else
        /* lowprobeval > target ; decrease midlow */
      {
        midlow = lowprobe - 1;
        lowdone = midlow < low;
      }
    }
  }
  high = ty - 1;
  midhigh = low;
  while (!highdone) {
    nialptr     yhigh = fetch_array(y, high);

    /* ensure the target is in the intervals */
    if (up(yhigh, target) && !equal(yhigh, target)) {
      highdone = true;
      low = high + 1;        /* to force Null result */
    }
    else
      /* reduce the interval at the high end */
    {
      nialint     highprobe = (high + midhigh + 1) / 2;
      nialptr     highprobeval = fetch_array(y, highprobe);

      if (equal(highprobeval, target)) {
        midhigh = highprobe;
        highdone = high == highprobe;
      }
      else if (!up(highprobeval, target)) {
        high = highprobe - 1;
        highdone = high < 0;
      }
      else
        /* highprobe < target */
      {
        midhigh = highprobe + 1;
        highdone = midhigh > high;
      }
    }
  }
  if (firstonly) {           /* set up a "seek" result */
    apush(createbool(low <= high));
    if (low <= high) {
      if (v == 1) {
        apush(createint(low));
      }
      else {
        apush(ToAddress(low, shpptr(y, v), v));
      }
    }
    else {
      if (v == 1) {
        apush(createint(ty));
        freeup(y);
      }
      else {
        apush(y);
        ishape();
      }
    }
    mklist(2);
  }
  else {                     /* set up a "findall" result */
    if (low > high) {
      apush(Null);
    }
    else {
      nialint     i,
                  size = high - low + 1;
      nialptr     z;

      if (v == 1) {
        z = new_create_array(inttype, 1, 0, &size);
        for (i = 0; i < size; i++)
          store_int(z, i, low + i);
        apush(z);
      }
      else {
        z = new_create_array(atype, 1, 0, &size);
        for (i = 0; i < size; i++)
          store_array(z, i, ToAddress(low + i, shpptr(y, v), v));
        apush(z);
      }
    }
  }
  swap();
  freeup(apop());            /* free protected x */
  freeup(y);
}


/* The next set of routines implement the operations:
       cull, that eliminates duplicates, and
       diverse, that determines if all the items of an array
            are different from each other.

   There are two internal versions:
       cull, described below, and
       scull, that assumes the array is sorted.
   Both of these have a parameter that determines whether the 
   result is for cull or diverse.

   The implementation of cull uses sorting to achieve an N*(log N)
   order algorithm. It is described in Nial by:

  cull is op A {
    Sh B := [shape,list] A;
    ord := GRADE up B;
    C := ord choose B;
    E F := [1 drop,-1 drop] C;
    Pattern := l hitch not (E match F);
    SORT <= (Pattern sublist Ord) choose B }

    The internal code computes Pattern using a loop.
    When the array is sorted, scull is an order N algorithm.

 */

void
icull()
{
  nialptr     z;

  z = apop();
  if (is_sorted(z) || check_sorted(z))
    scull(z, false);         /* use version that assumes sorted z */
  else
    cull(z, false);          /* assumes cull frees z */
}

void
idiverse(void)
{
  nialptr     z;
  z = apop();

  if (is_sorted(z) || check_sorted(z))
    scull(z, true);          /* use version that assumes sorted z */
  else
    cull(z, true);
}


void
cull(nialptr a, int diversesw)
{
    nialint     i,
    t = tally(a);
    nialptr     ord,
    ordereda;
    nialptr     pattern,
    indices;
    
    apush(a);                  /* list it so indices are simpler */
    ilist();
    a = top;                   /* leave a on stack to protect it for latter use */
    sort(upcode, a, true);
    ord = top;                 /* leave ord protected */
    choose(a, ord);
    ordereda = apop();
    ord = apop();
    pattern = new_create_array(booltype, 1, 0, &t);
    store_bool(pattern, 0, true);
    
    switch (kind(a)) {
        case booltype:
            for (i = 1; i < t; i++) {
                /* These following two line have been expanded out to prevent a
                 * compiler bug in BC 4.52 where the compiler just stopped
                 * compiling after this routine */
                int         k = fetch_bool(ordereda, i);
                int         j = fetch_bool(ordereda, i - 1);
                
                store_bool(pattern, i, k != j);
            }
            break;
        case inttype:
            for (i = 1; i < t; i++)
                store_bool(pattern, i, fetch_int(ordereda, i) !=
                           fetch_int(ordereda, i - 1));
            break;
        case realtype:
            for (i = 1; i < t; i++)
                store_bool(pattern, i, fetch_real(ordereda, i) !=
                           fetch_real(ordereda, i - 1));
            break;
        case chartype:
            for (i = 1; i < t; i++)
                store_bool(pattern, i, fetch_char(ordereda, i) !=
                           fetch_char(ordereda, i - 1));
            break;
        case atype:
            for (i = 1; i < t; i++) {
                int         test = equal(fetch_array(ordereda, i), fetch_array(ordereda, i - 1));
                
                store_bool(pattern, i, !test);
            }
            break;
        default:
            break;
    }
    
    
    if (diversesw == true) {
        freeup(apop());          /* unprotect a */
        freeup(ordereda);
        freeup(ord);
        /* a is diverse if there are no false's in pattern */
        apush(pattern);
        iand();
        
    }
    else {
        sublist(pattern, ord);   /* cleans up pattern and ord */
        indices = apop();
        /* sort the indices to get inverse permutation */
        sort(ltecode, indices, false);
        choose(a, apop());
        swap();
        freeup(ordereda);
        freeup(apop());          /* unprotect a */
    }
}


static void
scull(nialptr a, int diversesw)
{
  nialint     i,
              t = tally(a);
  int         k = (t==0 ? atype : booltype);
  nialptr     pattern = new_create_array(k, 1, 0, &t);

  if (t>0)
    store_bool(pattern, 0, true);
  switch (kind(a)) {
    case booltype:
        for (i = 1; i < t; i++)
          store_bool(pattern, i, fetch_bool(a, i) !=
                     fetch_bool(a, i - 1));
        break;
    case inttype:
        for (i = 1; i < t; i++)
          store_bool(pattern, i, fetch_int(a, i) !=
                     fetch_int(a, i - 1));
        break;
    case realtype:
        for (i = 1; i < t; i++)
          store_bool(pattern, i, fetch_real(a, i) !=
                     fetch_real(a, i - 1));
        break;
    case chartype:
        for (i = 1; i < t; i++)
          store_bool(pattern, i, fetch_char(a, i) !=
                     fetch_char(a, i - 1));
        break;
    case atype:
        for (i = 1; i < t; i++) {
          int         test = equal(fetch_array(a, i), fetch_array(a, i - 1));

          store_bool(pattern, i, !test);
        }
        break;
  }
  if (diversesw) {           /* a is diverse if there are no false's in
                              * pattern */
    freeup(a);
    apush(pattern);
    iand();
  }
  else
    sublist(pattern, a);     /* cleans up pattern and a and does set_sorted */

}


/* The following routines implement the primitive operation except
   where A except B returns the list of items of A not in B. 

   The straightforward algorithm is order N**2 algorithm. 
   It is used for small arrays. There are three internal routines:
      except, which sorts the arrays to achieve an order N*log N algoritm,
      sexcept, which assumes the arrays are sorted, and
      oldexcept, which uses the double loop algorithm.

   The routine except is described by the Nial code:

   except is op A B {
   A := list A;
   B := cull B;
   if simple A THEN
   ordA := GRADE <= A;
   else
   ordA := GRADE up A;
   endif;
   if simple B THEN
   B := SORT <= B;
   else
   B := SORT up B;
   endif;
   C := OrdA choose A;
   I := 0; J := 0;
   t2 gets time; write "preptime (t2 - t1);
  Res := Null;
  WHILE I < tally A and (J < tally B) DO
    IF C@I up B@J THEN
      IF C@I ~= B@J THEN
        Res := Res append OrdA@I;
      ENDIF;
      I := I + 1;
    ELSE
      J := J + 1;
    ENDIF;
  ENDWHILE;
  Res := Res link (I drop OrdA);
 Res := SORT <= Res choose A;
  Res }

   The constant CROSSOVER determines the sizes of arrays A and B that are computed
   using the N*log N algorithm. Its value was determined experimentally
   on a 64 bit Intel chip for OSX (in a 2009 era Mac).

   If the result is empty then it is the array Null.
*/

#define CROSSOVER 5000


void
b_except(void)
{
  nialptr     y = apop(),
              x = apop();

  if (!(is_sorted(x) || check_sorted(x))) {
    if (tally(x) == 1 || (tally(x) * tally(y) <= CROSSOVER))
      oldexcept(x, y);
    else
      except(x, y);
  }
  else {
    if (!(is_sorted(y) || check_sorted(y))) {
      sort(upcode, y, false);
      y = apop();
    }
    sexcept(x, y);
  }
}

#ifdef OLDEXCEPT

/* temporary version to test cross over value */
/* to do the testing 
   1) add an entry in pkgblder/allprims.nh
 OLDEXCEPT U oldexcept ioldexcept
   2) add OLDEXCEPT to the .txt file being used for the build
   3) rebuild the executable.
   4) do timing comparisons between except and oldexcept on varying size arrays.
 */

void
ioldexcept(void)
{
  nialptr     z,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of oldexcept must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    oldexcept(x, y);
  }
  freeup(z);
}

#endif  /* OLDEXCEPT */

void
iexcept(void)
{
  nialptr     z,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of except must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    if (!(is_sorted(x) || check_sorted(x))) {
      if (tally(x) == 1 || (tally(x) * tally(y) <= CROSSOVER))
        oldexcept(x, y);
      else
        except(x, y);
    }
    else {
      if (!(is_sorted(y) || check_sorted(y))) {
        sort(upcode, y, false);
        y = apop();
      }
      sexcept(x, y);
    }
  }
  freeup(z);
}

/* internal routine to check if an array is sorted by "up" and to set
   the sorted flag for the array accordingly.
*/

int
check_sorted(nialptr x)
{
  nialint     i,
              n = tally(x);
  int         sortedflag = true,
              kx = kind(x);

  if (n >= 2) {
    i = 0;
    while (sortedflag && i < n - 1) {
      switch (kx) {
        case booltype:
            sortedflag = fetch_bool(x, i) <= fetch_bool(x, i + 1);
            break;
        case inttype:
            sortedflag = fetch_int(x, i) <= fetch_int(x, i + 1);
            break;
        case realtype:
            sortedflag = fetch_real(x, i) <= fetch_real(x, i + 1);
            break;
        case chartype:
            {
              int         c0 = invseq[fetch_char(x, i) - LOWCHAR],
                          c1 = invseq[fetch_char(x, i + 1) - LOWCHAR];

              sortedflag = c0 <= c1;
            }
            break;
        case atype:
            sortedflag = up(fetch_array(x, i), fetch_array(x, i + 1));
            break;
      }
      i++;
    }
    if (sortedflag)
      set_sorted(x, true);
  }
  return sortedflag;
}


static void
except(nialptr a, nialptr b)
{
  nialptr     ord,
              ordereda,
              indices;
  nialint     i,
              j,
              cnt,
              ta,
              tb;


  apush(a);                  /* list it so indices are simpler and there are
                              * no atomic cases */
  ilist();
  a = top;                   /* leave a on stack to protect it for latter use */
  sort(upcode, a, true);
  ord = top;                 /* leave ord protected during the choose */
  choose(a, ord);
  ordereda = apop();
  ord = apop();
  cull(b, false);
  b = apop();
  sort(upcode, b, false);
  b = apop();

  ta = tally(ordereda);
  tb = tally(b);

  /* ensure that ordereda and b are the same homotype or atype. */
  if (kind(a) != kind(b)) {
    if (homotype(kind(ordereda)))
      ordereda = explode(ordereda, 1, ta, 0, ta);
    if (homotype(kind(b)))
      b = explode(b, 1, tb, 0, tb);
  }

  /* walk ordered a and b keeping indices of values in a not in b */
  i = 0;
  j = 0;
  cnt = 0;

  indices = new_create_array(inttype, 1, 0, &ta);
 
 /* do the homogeneous cases directly */
  switch (kind(ordereda)) {
    case booltype:
        while (i < ta && j < tb) {
          int         itema = fetch_bool(ordereda, i),
                      itemb = fetch_bool(b, j);

          if (itema <= itemb) {
            if (itema != itemb) {
              store_int(indices, cnt, fetch_int(ord, i));
              cnt++;
            }
            i++;
          }
          else
            j++;
        }
        while (i < ta) {
          store_int(indices, cnt, fetch_int(ord, i));
          cnt++;
          i++;
        }
        break;
    case inttype:
        while (i < ta && j < tb) {
          nialint     itema = fetch_int(ordereda, i),
                      itemb = fetch_int(b, j);

          if (itema <= itemb) {
            if (itema != itemb) {
              store_int(indices, cnt, fetch_int(ord, i));
              cnt++;
            }
            i++;
          }
          else
            j++;
        }
        while (i < ta) {
          store_int(indices, cnt, fetch_int(ord, i));
          cnt++;
          i++;
        }
        break;
    case realtype:
        while (i < ta && j < tb) {
          double      itema = fetch_real(ordereda, i),
                      itemb = fetch_real(b, j);

          if (itema <= itemb) {
            if (itema != itemb) {
              store_int(indices, cnt, fetch_int(ord, i));
              cnt++;
            }
            i++;
          }
          else
            j++;
        }
        while (i < ta) {
          store_int(indices, cnt, fetch_int(ord, i));
          cnt++;
          i++;
        }
        break;
    case chartype:
        while (i < ta && j < tb) {
          char        itema = fetch_char(ordereda, i),
                      itemb = fetch_char(b, j);

          if (invseq[itema - LOWCHAR] <= invseq[itemb - LOWCHAR]) {
            if (itema != itemb) {
              store_int(indices, cnt, fetch_int(ord, i));
              cnt++;
            }
            i++;
          }
          else
            j++;
        }
        while (i < ta) {
          store_int(indices, cnt, fetch_int(ord, i));
          cnt++;
          i++;
        }
        break;
    case atype:
        while (i < ta && j < tb) {
          nialptr     itema = fetch_array(ordereda, i),
                      itemb = fetch_array(b, j);
          int         val;

          val = up(itema, itemb);
          if (val) {
            if (!equal(itema, itemb)) {
              store_int(indices, cnt, fetch_int(ord, i));
              cnt++;
            }
            i++;
          }
          else
            j++;
        }
        while (i < ta) {
          store_int(indices, cnt, fetch_int(ord, i));
          cnt++;
          i++;
        }
        break;
  }

  /* set up index array for results */
  if (cnt < ta) {
    nialptr     newind = new_create_array(inttype, 1, 0, &cnt);

    copy(newind, 0, indices, 0, cnt);
    freeup(indices);
    indices = newind;
  }

  /* sort the indices to get inverse permutation */
  sort(ltecode, indices, false);  /* frees indices */
  choose(a, apop());
  swap();
  freeup(apop());            /* to unprotect a */
  freeup(ordereda);
  freeup(b);
  freeup(ord);
}


static void
sexcept(nialptr a, nialptr b)
{
  nialptr     z;
  nialint     i,
              j,
              cnt,
              ta,
              tb;
  int         ka,
              kb;

  apush(a);
  ilist();                   /* ensures there are not atoms to consider */
  a = apop();

  ta = tally(a);
  tb = tally(b);
  if (tb == 0) {
    apush(a);
    freeup(b);
    return;
  }
  /* ensure a and b are of the same homotype or are atype */
  ka = kind(a);
  kb = kind(b);
  if (ka != kb) {
    if (homotype(ka)) {
      a = explode(a, 1, ta, 0, ta);
      ka = kind(a);
    }
    if (homotype(kb))
      b = explode(b, 1, tb, 0, tb);
  }
  /* walk ordered a and b keeping values in a not in b */
  i = 0;
  j = 0;
  cnt = 0;
  /* set up result container at maximum size */
  z = new_create_array(ka, 1, 0, &ta);

  /* do the homogeneous cases directly */
  switch (ka) {
    case booltype:
        while (i < ta && j < tb) {
          int         itema = fetch_bool(a, i),
                      itemb = fetch_bool(b, j);

          if (itema <= itemb) {
            if (itema != itemb) {
              store_bool(z, cnt, itema);
              cnt++;
            }
            i++;
          }
          else
            j++;
        }
        /* need to move rest of list */
        while (i < ta) {
          store_bool(z, cnt, fetch_bool(a, i));
          cnt++;
          i++;
        }
        break;
    case inttype:
        while (i < ta && j < tb) {
          nialint     itema = fetch_int(a, i),
                      itemb = fetch_int(b, j);

          if (itema <= itemb) {
            if (itema != itemb) {
              store_int(z, cnt, itema);
              cnt++;
            }
            i++;
          }
          else
            j++;
        }
        /* need to move rest of list */
        while (i < ta) {
          store_int(z, cnt, fetch_int(a, i));
          cnt++;
          i++;
        }
        break;
    case realtype:
        while (i < ta && j < tb) {
          double      itema = fetch_real(a, i),
                      itemb = fetch_real(b, j);

          if (itema <= itemb) {
            if (itema != itemb) {
              store_real(z, cnt, itema);
              cnt++;
            }
            i++;
          }
          else
            j++;
        }
        /* need to move rest of list */
        while (i < ta) {
          store_real(z, cnt, fetch_real(a, i));
          cnt++;
          i++;
        }
        break;
    case chartype:
        while (i < ta && j < tb) {
          char        itema = fetch_char(a, i),
                      itemb = fetch_char(b, j);

          if (invseq[itema - LOWCHAR] <= invseq[itemb - LOWCHAR]) {
            if (itema != itemb) {
              store_char(z, cnt, itema);
              cnt++;
            }
            i++;
          }
          else
            j++;
        }
        /* need to move rest of list */
        while (i < ta) {
          store_char(z, cnt, fetch_char(a, i));
          cnt++;
          i++;
        }
        break;
    case atype:
        while (i < ta && j < tb) {
          nialptr     itema = fetch_array(a, i),
                      itemb = fetchasarray(b, j);
          int         val;

          apush(itemb);      /* to protect it in up */
          val = up(itema, itemb);
          if (val) {
            if (!equal(itema, itemb)) {
              store_array(z, cnt, itema);
              cnt++;
            }
            i++;
          }
          else
            j++;
          freeup(apop());    /* to unprotect and free itemb */
        }
        /* need to move rest of list */
        while (i < ta) {
          store_array(z, cnt, fetch_array(a, i));
          cnt++;
          i++;
        }
        break;
  }
  if (cnt < ta) 
  {            /* move result to a shorter container */
    nialptr res;
    if (cnt==0)
      res = Null;
    else
    { res = new_create_array(ka, 1, 0, &cnt);
      copy(res, 0, z, 0, cnt);
    }
    freeup(z);
    z = res;
  }
  if (homotest(z))
    z = implode(z);          /* this is necessary if a was exploded above */
  set_sorted(z, true);       /* since a was sorted */
  apush(z);
  freeup(a);
  freeup(b);
}

/* We use the old code when the args are small */

static void
oldexcept(nialptr arr, nialptr x)
{
  nialptr     nz,
              it;
  nialint     tarr,
              tx,
              i,
              j,
              clearplace;
  int         ka = kind(arr);

  tarr = tally(arr);
  tx = tally(x);
  if (tarr == 0) {
    apush(arr);
    ilist();
    freeup(x);
    return;
  }
  if (atomic(x)) {
    apush(x);                /* needed to avoid equal removing x when it is
                              * atomic */
    ilist();
    x = apop();
  }
  if (atomic(arr)) {
    apush(arr);              /* needed to avoid equal removing arr when it is
                              * atomic */
    ilist();
    arr = apop();
    ka = kind(arr);
  }
  nz = new_create_array(ka, 1, 0, &tarr);
  clearplace = 0;
  for (i = 0; i < tarr; i++) {  /* check for each item in arr */
    it = fetchasarray(arr, i);
    apush(it);               /* protect during equal */
    j = 0;
    while ((j < tx) && (!equal(fetchasarray(x, j), it)))
      j++;
    if (j == tx) {           /* item not found in x */
      copy1(nz, clearplace, arr, i);
      clearplace++;
    }
    freeup(apop());          /* free protected item in it */
#ifdef USER_BREAK_FLAG
    checksignal(NC_CS_NORMAL);
#endif
  }
  if (clearplace != tarr)
  {            /* move result to a shorter container */
    nialptr nnz;
    if (clearplace==0)
      nnz = Null;
    else
    { nnz = new_create_array(ka, 1, 0, &clearplace);
      copy(nnz, 0, nz, 0, clearplace);
    }
    if (homotest(nnz))
      nnz = implode(nnz);    /* in case all non-atomics eliminated */
    apush(nnz);
    freeup(nz);
  }
  else
    apush(nz);
  freeup(arr);
  freeup(x);
}




/* routine to implement the primitive predicate: atomic */

void
iatomic(void)
{                            /* atomic */
  nialptr     z;

  z = apop();
  if (atomic(z)) {
    apush(True_val);
  }
  else
    apush(False_val);
  freeup(z);
}


/* pass is the operation that returns its argument */

void
ipass(void)
{
}                            /* pass */

void
ivalence(void)
{                            /* valence */
  nialptr     z;

  z = apop();
  apush(createint((nialint) valence(z))); 
  freeup(z);
}


/* The next section of code implements the NIAL primitives 
   for axis manipulation:
       N raise A, which lifts the first N axes of A,
       I fuse A, which remaps the axes of A according to I, and
       transpose A, which reverses the axes of A.
   
   The internal routine for raise is nial_raise.  

*/

void
b_raise(void)
{
  nialptr     y = apop(),
              x = apop();

/* raise is only valid if its left arg is an int >=0 and <= valence y) */
  if (kind(x) != inttype)
    buildfault("invalid arg in raise");
  else {
    nialint     n = intval(x);

    if (n < 0 || n > valence(y))
      buildfault("invalid arg in raise");
    else
      apush(nial_raise(y, n));
  }
  freeup(x);
  freeup(y);                 /* nial_raise does not free y */
}



void
iraise(void)
{
  nialptr     x,
              y,
              z;
  nialint     n;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of raise must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
/* raise is only valid if its left arg is an int >=0 and <= valence y) */
    if (kind(x) != inttype)
      buildfault("first arg of raise not an axis number");
    else {
      n = intval(x);
      if (n < 0 || n > valence(y))
        buildfault("first arg of raise not an axis number");
      else
        apush(nial_raise(y, n));
    }
    freeup(x);
    freeup(y);               /* nial_raise does not free y */
  }
  freeup(z);
}

/* nial_raise(x,n) splits x into subarrays using n as the control var.
   The result has shape:   n take shape x.
   The items have shape:   n drop shape x.
   It does the work for raise. It is needed in this form for picture.
   If A is empty and (N take shape A) does not contain zeros
   then the items of A are all empty arrays.
*/

nialptr
nial_raise(nialptr x, int n)
{
  nialptr     w,
              z;
  nialint     cnt,
              cnt1,
              i,
              k;
  int         v,
              kx,
              n1;

  if (atomic(x))
    z = x;
  else {
    v = valence(x);
    if (n == v)
    { if (homotype(kind(x)))
        z = x;
      else 
      { apush(x);            /* to protect x during each */
        int_each(isingle, x);
        z = apop();
        apop();              /* to unprotect x */
      }
    }
    else 
    { nialint    *sh,
                 *sh1;

      sh = shpptr(x, v);     /* Safe. Only used in cnt and cnt1 below. */
      cnt = 1;
      for (i = 0; i < n; i++)
        cnt *= *(sh + i);
      sh1 = sh + n;
      n1 = v - n;
      cnt1 = 1;
      for (i = 0; i < n1; i++)
        cnt1 *= *(sh1 + i);
      z = new_create_array(atype, n, 0, shpptr(x, v));
      k = 0;                 /* counter to walk through x */
      kx = kind(x);
      for (i = 0; i < cnt; i++) 
      { w = new_create_array(kx, n1, 0, shpptr(x, v) + n);
        { copy(w, 0, x, k, cnt1); /* copy cnt1 items */
          k += cnt1;
        }
        if (homotest(w))     /* in case a partition is homogeneous */
          w = implode(w);
        store_array(z, i, w);
#ifdef USER_BREAK_FLAG
        checksignal(NC_CS_NORMAL);
#endif
      }
      /* freeup is done done in raise and paste */
    }
  }
  return (z);
}

/* The operation I fuse A is used for two distinct purposes.  
   If I is simple and contains all the axes of A without repetition, 
   the result is an array formed by a permutation of the axes of A by I.  
   The shape of the result is I choose shape A.  
   If I is not simple but link I is simple and contains all of the axes 
   of A without repetition, the result is obtained by diagonalizing 
   along axes that are grouped together, ordering them according 
   to the ordering in I. If link I does not contain all the axes 
   or if there are repetitions of the axes in link I, the 
   fault ?invalid fuse is returned. 

   I is a direct coding for the axes. 
   The items of the left arg indicate where the axes of the right 
   arg are to be place. If an item is a list of integers,
   then the indicated axes are to be combined by selecting 
   items from the main diagonal along those sets of axes.
   
For a matrix x
      0 1 fuse x = x
      1 0 fuse x = transpose x
      [0 1] fuse x = principal diagonal as a list
*/

void
b_fuse(void)
{
  nialptr     y = apop(),
              x = apop();

  fuse(x, y);
}

void
ifuse(void)
{
  nialptr     z,
              x,
              y;
  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of fuse must be a pair"));
  }
  else {
    splitfb(z, &x, &y);
    fuse(x, y);
  }
  freeup(z);

}

/* routine to do the work of fuse. Algorithm is:

 1) handle the cases of valence 0 and 1, which return the right arg for
    a valid encoding.
 2) construct the shape of the result by selecting items from the
    shape of b. If axes are being fused, the minimal sized extent is
    used.
 3) construct the permutation to map addresses of the result to the
    appropriates addresses of b.
 4) construct the result container and loop over its addresses using
    the permutation to select the corresponding addresses of b.
    This computation is done by computing the strides of b associated
    with the axes of the result according to the permutation vector.
*/

static void
fuse(nialptr a, nialptr b)
{
  nialptr     sb,
              ns,
              ai,
              result,
              check;
  nialint     tb,
              ta,
              i,
              aiv,
              nsi = 0,
              minn,
              j,
              tr,
              index;
  int         kb,
              ka,
              test,
              v;
  nialint     vextent[1];

  ta = tally(a);
  ka = kind(a);
  v = valence(b);
  tb = tally(b);
  vextent[0] = (nialint)v;
    
  apush(b);                  /* to protect b in ishape */
  apush(b);
  ishape();
  sb = apop();
  apop();                    /* to unprotect b */
  if (v == 0)
  {             /* if b is a single then left arg must be empty 
                   or a solitary empty */
    if (ta == 0 || (ka == atype && ta == 1 && tally(fetch_array(a,0)==0))) 
    {
      apush(b);
    }
    else {
      buildfault("left arg of fuse should be Null");
      goto cleanup;
    }
  }
  else if (v == 1) {        /* if b is a list the left arg of must be zero */
    if (tally(a) == 1 && kind(a) == inttype && fetch_int(a, 0) == 0) {
      apush(b);
    }
    else {
      buildfault("left arg of fuse should be 0");
      goto cleanup;
    }
  }
  else {                     /* b has at least 2 axes */
    if (ka != inttype && ka != atype) {
      buildfault("invalid left arg of fuse");
      goto cleanup;
    }
    if (ka == inttype && ta != (nialint)v) {  /* tally of a must be same as valence
                                       * of b if a is all integers */
      buildfault("left arg of fuse should contain axes of right arg");
      goto cleanup;
    }                        /* build new shape using coding in left
                              * argument. Check validity of left argument by
                              * seeing that each axis of b is used exactly
                              * once */
    check = new_create_array(booltype, 1, 0, vextent);
    for (i = 0; i < v; i++)
      store_bool(check, i, false);
    ns = new_create_array(inttype, 1, 0, &ta);

    for (i = 0; i < ta; i++) {
      if (ka == inttype) {   /* no fusing of axes */
        aiv = fetch_int(a, i);
        if (aiv < 0 || aiv >= v || fetch_bool(check, aiv)) {
          buildfault("left arg of fuse should contain axes of right arg");
          freeup(ns);
          freeup(check);
          goto cleanup;
        }
        store_bool(check, aiv, true);
        nsi = fetch_int(sb, aiv);
      }
      else if (ka == atype) {/* axes to be fused */
        ai = fetch_array(a, i);
        if (kind(ai) == inttype) {
          minn = LARGEINT;
          for (j = 0; j < tally(ai); j++) {
            aiv = fetch_int(ai, j);
            if (aiv < 0 || aiv >= v || fetch_bool(check, aiv)) {
              buildfault("left arg of fuse should contain axes of right arg");
              freeup(ns);
              freeup(check);
              goto cleanup;
            }
            store_bool(check, aiv, true);
            nsi = fetch_int(sb, aiv);
            if (nsi < minn)
              minn = nsi;
          }
          nsi = minn;
        }
        else {
          buildfault("invalid fuse");
          freeup(ns);
          freeup(check);
          goto cleanup;
        }
      }
      store_int(ns, i, nsi);
    }
    apush(check);

    iand();

    test = boolval(top);
    freeup(apop());
    if (!test) {             /* not all axes were used */
      buildfault("left arg of fuse should contain axes of right arg");
      freeup(ns);
      goto cleanup;
    }
    if (tb == 0)
      /* result is empty of new shape */

    {
      nialptr z = new_create_array(atype, tally(ns), 0, pfirstint(ns));
      apush(z);
    }
    else {                   /* code to do the actual tranposition quickly */
      nialptr     ai,
                  toaddr,
                  strides,
                  newstrides;
      nialint    *shp;
      int         processaxes;
      nialint     prod,
                  amt,
                  pos,
                  stride,
                  offset,
                  limit,
                  dim;

      /* create the result container. */
      kb = kind(b);

      result = new_create_array(kb, tally(ns), 0, pfirstint(ns));

      tr = tally(result);
      /* create an address to range over the addresses of the result and
       * initialize it to zeros. */
      toaddr = new_create_array(inttype, 1, 0, &ta);

      for (i = 0; i < ta; i++)
        store_int(toaddr, i, 0);
      /* create the vector of strides for b  */
      strides = new_create_array(inttype, 1, 0, vextent);

      shp = shpptr(b, v);   /* Safe. Done after heap creates */
      prod = 1;
      for (i = v - 1; i >= 0; i--) {
        store_int(strides, i, prod);
        prod *= *(shp + i);
      }
      /* compute the newstrides in b corresponding to the axes in the result.
       * This is done by using the argument a to select strides, adding
       * together those for a diagonalization. */
      newstrides = new_create_array(inttype, 1, 0, &ta);

      for (i = 0; i < ta; i++) {
        if (ka == inttype)   /* no fusing of axes */
          store_int(newstrides, i, fetch_int(strides, fetch_int(a, i)));
        else {
          amt = 0;
          ai = fetch_array(a, i);
          for (j = 0; j < tally(ai); j++)
            amt += fetch_int(strides, fetch_int(ai, j));
          store_int(newstrides, i, amt);
        }
      }

      /* the stride is based on the last axis */
      stride = fetch_int(newstrides, ta - 1);
      limit = fetch_int(ns, ta - 1);
      pos = 0;               /* cycles over the last axis of the result */
      offset = 0;            /* no offset initially */
      /* loop to walk over indices of the result */
      for (i = 0; i < tr; i++) {  /* compute the index for the item of b and
                                   * move it to the result */
        index = offset + pos * stride;
        copy1(result, i, b, index);
        pos++;
        if (pos == limit) {  /* loop over final axis is completed. do the end
                              * of axes processing */
          processaxes = true;
          dim = ta - 2;      /* other axes are incremented and checked */
          while (processaxes && dim >= 0) { /* increment the address in the
                                             * current dimension */
            store_int(toaddr, dim, 1 + fetch_int(toaddr, dim));
            if (fetch_int(toaddr, dim) == fetch_int(ns, dim)) {
              store_int(toaddr, dim, 0);  /* reset dim address position */
              /* compute the new offset */
              offset = 0;
              for (j = 0; j < dim; j++)
                offset += fetch_int(newstrides, j) * fetch_int(toaddr, j);

              dim--;         /* stay in the while loop to set the next lower
                              * axis */
            }
            else {
              processaxes = false;
              offset += fetch_int(newstrides, dim); /* increment the offset */
            }
          }
          pos = 0;           /* reset pos to walk last axis again */
        }
      }
      freeup(strides);
      freeup(newstrides);
      freeup(toaddr);
      apush(result);
    }
    freeup(ns);
  }

cleanup:
  freeup(sb);
  freeup(a);
  freeup(b);

}

/* the routine to implement the Nial primitive transpose.
   It is done directly for a 2-dimensional array (matrix) and
   by the identity
         transpose A = reverse A axes A fuse A
   if valence A > 2.
*/

void
itranspose(void)
{
  nialptr     x,
              reverseaxis;
  nialint     vx,
              i;

  x = apop();
  vx = valence(x);
  if (vx <= 1) {
    apush(x);
  }
  else if (vx == 2) {
    nialint     i,
                j,
                k,
                p,
                r,
                c,
                tx = tally(x),
               *shp = shpptr(x, vx),  /* Safe. only used in r and c below */
                newshp[2];
    nialptr     z;

    r = shp[0];
    c = shp[1];
    newshp[0] = c;
    newshp[1] = r;
    z = new_create_array(kind(x), 2, 0, newshp);
    switch (kind(x)) {
      case chartype:
          {
            char       *xp = pfirstchar(x);

            i = 0;
            j = 0;
            for (k = 0; k < tx; k++) {
              p = j * r + i;
              store_char(z, p, xp[k]);
              j++;
              if (j == c) {
                i++;
                j = 0;
              }
            }
            break;
          }
      case inttype:
          {
            nialint    *xp = pfirstint(x);

            i = 0;
            j = 0;
            for (k = 0; k < tx; k++) {
              p = j * r + i;
              store_int(z, p, xp[k]);
              j++;
              if (j == c) {
                i++;
                j = 0;
              }
            }
            break;
          }
      case realtype:
          {
            double     *xp = pfirstreal(x);

            i = 0;
            j = 0;
            for (k = 0; k < tx; k++) {
              p = j * r + i;
              store_real(z, p, xp[k]);
              j++;
              if (j == c) {
                i++;
                j = 0;
              }
            }
            break;
          }
      case booltype:
          {
            i = 0;
            j = 0;
            for (k = 0; k < tx; k++) {
              p = j * r + i;
              store_bool(z, p, fetch_bool(x, k));
              j++;
              if (j == c) {
                i++;
                j = 0;
              }
            }
            break;
          }
      case atype:
          {
            nialptr    *xp = pfirstitem(x);

            i = 0;
            j = 0;
            for (k = 0; k < tx; k++) {
              p = j * r + i;
              store_array(z, p, xp[k]);
              j++;
              if (j == c) {
                i++;
                j = 0;
              }
            }
            break;
          }
    }
    apush(z);
    freeup(x);
  }
  else {
    reverseaxis = new_create_array(inttype, 1, 0, &vx);
    for (i = 0; i < vx; i++)
      store_int(reverseaxis, i, vx - 1 - i);
    fuse(reverseaxis, x);
  }
}

/* routine to implement reverse */

void
ireverse()
{
  nialptr     x = apop();
  int         v = valence(x);

  if (v == 0 || tally(x) == 0) {
    apush(x);
  }
  else {
    nialptr     z = new_create_array(kind(x), v, 0, shpptr(x, v));
    nialint     i,
                tm1 = tally(x) - 1;

    for (i = 0; i < tally(x); i++)
      copy1(z, i, x, tm1 - i);
    apush(z);
    freeup(x);
  }
}


/* primitive Nial predicate empty */

void
iempty(void)
{
  nialptr     x;

  x = apop();
  if (tally(x) == 0) {
    apush(True_val);
  }
  else
    apush(False_val);
  freeup(x);
}

/* routine to implement mix defined by:

mix IS OPERATION A {
   IF empty A THEN
     shape A append 0 reshape A
   ELSEIF not equal EACH shape A THEN
     ??conform
   ELSE
     shape A link shape first A reshape link A
   ENDIF }

*/

void
imix(void)
{
  nialptr shp;
  nialptr bres;
  int tv;

  apush(top); /* duplicate top so we can get its shape */
  ishape();
  shp = apop();  /* shape A */

  if (tally(top)==0)
  { freeup(apop());  /* unprotect and free arg */
    apush(shp);
    apush(Zero);
    b_append();   /* shape A append 0 */
    apush(Null);   /* reshape Null since A has no items */
    b_reshape();  /* shape A append 0 reshape Null */
    return;
  } 

  /* test for equishaped items */
 
  int_each(ishape,top);
  iequal();
  bres = apop();
  tv = boolval(bres); 
  freeup(bres);
  if (!tv)
  { freeup(apop()); /* unprotect and free arg */
    freeup(shp);
    buildfault("conform");
    return;
  }

  apush(fetchasarray(top,0)); /* first A  */
  ishape();  /* shape first A */
  pair(shp, apop());
  ilink();  /* shape A link shape first A */
  swap();   /* puts A on top */
  ilink();  /* link A */
  b_reshape();  /* shape A link shape first A reshape link A */
}
     
