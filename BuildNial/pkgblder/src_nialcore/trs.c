/* ==============================================================

   MODULE     TRS.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This module implements all the primitive transformers.


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

/* SJLIB */
#include <setjmp.h>

/* MALLOCLIB */
#include <stdlib.h>

/* Q'Nial header files */

#include "trs.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"
#include "if.h"

#include "eval.h"            /* for apply */
#include "faults.h"          /* for Logical */
#include "ops.h"             /* for simple up etc. */
#include "compare.h"         /* for up */
#include "blders.h"          /* for getters and builders */
#include "getters.h"         /* for getters */
#include "parse.h"           /* for parse tree node tags */
#include "arith.h"           /* for prodints */
#include "insel.h"           /* for choose */
#include "profile.h"         /* for profile switch */
#include "nialconsts.h"	     /* for INTS32 or INTS64 switch */


static void each(nialptr f, nialptr x);
static void eachboth(nialptr f, nialptr x, nialptr y);
static void reduce(nialptr f);
static void leftaccumulate(nialptr f);
static void accumulate(nialptr f);
static void rank(nialptr f, nialint n, nialptr a);


 /* each routine here implements a primitive transformer.  The convention is
    that the array argument is pushed first and then the operation argument.

    The operation argument may or may not be permanent code and is not to be
    freed. Routines that create temporary args and push them must free them
    up before returning, but must protect the given arg if it is part of the
    structure.
  
    The C variables leafcode, twigcode, and conversecode hold pointers to parse
    tree entries that are built during initialization. They are needed to
    build operation representations that are needed dynamically in the
    implementation of leaf, twig and eachleft respectively. 
*/

 /* routine to do each */

void
ieach()
{
  nialptr     f = apop(); /* the functional argument */

  if (tag(f) == t_basic) { /* f is a primitive Nial operation */


    if (get_prop(f) == 'P') { /* f is pervasive, apply directly */
      APPLYPRIMITIVE(f);

#ifdef FP_EXCEPTION_FLAG
      fp_checksignal();
#endif
    }

    else { /* use version of each with C code function */
      int_each(applytab[get_index(f)], apop());

#ifdef FP_EXCEPTION_FLAG
      fp_checksignal();
#endif
    }
  }
  else /* use version of each with Nial parse tree function */
    each(f, apop());
}

/* routine to implement the EACH transformer when f is a parse tree
   representation of a Nial operation.
*/

static void
each(nialptr f, nialptr x)
{
  nialptr     z,
              arg,
              firstres,
              res;
  nialint     i,
              tx = tally(x);
  int         kz,
              flag,
              v = valence(x);

  if (tx == 0) /* arg is empty */
  { /* arg is empty, return it as the result */
    apush(x);
    return;
  }

  if (v == 0) {              /* arg is a single */
    flag = atomic(x);
    arg = (flag ? x : fetch_array(x, 0)); /* use x directly if atomic, 
                                             otherwise use its item */
    apush(arg);
    do_apply(f);
    isingle();               /* isingle will do nothing if its arg is atomic */
    if (!flag)               /* x not used as argument in do_apply */
      freeup(x);
  }

  else {  /* at least one item */
    /* apply f to first item */
    apush(fetchasarray(x, 0));
    do_apply(f);
    firstres = apop();

    /* determine if the result is potentially homogeneous */
    kz = kind(firstres);
    kz = atomic(firstres) && homotype(kz) ? kz : atype;

    /* create the result container */
    z = new_create_array(kz, v, 0, shpptr(x, v));

    if (kz == atype) { /* store the Nial result */
      store_array(z, 0, firstres);  /* block is necessary for store_array macro */
    }
    else { /* copy the C value */
      copy1(z, 0, firstres, 0);
      freeup(firstres);
    }

    /* loop over the remaining items, applying f and checking
       that the result type remains the same */
    for (i = 1; i < tx; i++) {
      apush(fetchasarray(x, i));
      do_apply(f);
      res = apop();
      if (atomic(res) && kind(res) == kz) {
        copy1(z, i, res, 0);
        freeup(res);
      }

      else { /* non atomic result or atomic type has changed */
        if (kz == atype) {
          store_array(z, i, res); /* block is necessary for store_array macro  */
        }

        else { /* must blow up result container since kind has changed */
          z = explode(z, v, tx, 0, i);
          store_array(z, i, res);
          kz = atype;
        }
      }
    }  /* end of loop */

    if (homotest(z)) { /* result can be made homogeneous */
      incrrefcnt(x);   /* protect x in case it is an item of z in the implode */
      z = implode(z);
      decrrefcnt(x);   /* unprotect x */
    }
    apush(z);
    freeup(x);
  }
}

/* routine to implement the EACH transformer when f is the C function
   implementing a Nial operation. Similar to the above routine except uses
   (*f)()  instead of    do_apply(f) .
*/

void
int_each(void (*f) (void), nialptr x)
{
  nialptr     z,
              arg,
              firstres,
              res;
  nialint     i,
              tx = tally(x);
  int         kz,
              flag,
              v = valence(x);

  if (tx ==0)  /* arg is empty */
  { /* arg is empty, return it as the result */
    apush(x);
    return;
  }

  if (v == 0) {              /* arg is a single */
    flag = atomic(x);
    arg = (flag ? x : fetch_array(x, 0)); /* use x directly if atomic, 
                                             otherwise use its item */

    apush(arg);
    (*f) ();
    isingle();     /* isingle will do nothing if its arg is atomic */
    if (!flag)     /* x not used as argument in call on f */
      freeup(x);
  }

  else {  /* at least one item */
    /* apply f to first item */
    apush(fetchasarray(x, 0));
    (*f) ();
    firstres = apop();

    /* determine if the result is potentially homogeneous */
    kz = kind(firstres);
    kz = atomic(firstres) && homotype(kz) ? kz : atype;

    /* create the result container */
    z = new_create_array(kz, v, 0, shpptr(x, v));

    if (kz == atype) { /* store the Nial result */
      store_array(z, 0, firstres);  /* block is necessary for store_array macro */
    }
    else { /* copy the C value */
      copy1(z, 0, firstres, 0);
      freeup(firstres);
    }

    /* loop over the remaining items, applying f and checking
       that the result type remains the same */
    for (i = 1; i < tx; i++) {
      apush(fetchasarray(x, i));
      (*f) ();
      res = apop();
      if (atomic(res) && kind(res) == kz) {
        copy1(z, i, res, 0);
        freeup(res);
      }

      else { /* non atomic result or atomic type has changed */
        if (kz == atype) {
          store_array(z, i, res); /* block is necessary for store_array macro */
        }
        else { /* must blow up result container since kind has changed */
          z = explode(z, v, tx, 0, i);
          store_array(z, i, res);
          kz = atype;
        }
      }
    }
    if (homotest(z)) { /* result can be made homogeneous */
      incrrefcnt(x);   /* protect x in case it is an item of z in the implode */
      z = implode(z);
      decrrefcnt(x);         /* unprotect x */
    }
    apush(z);
    freeup(x);
  }
}

/* routine to implement the EACH transformer when f is the C function
   that maps a double to a double. It is used in the scientific primitives
   called in trig.c .
*/

void
real_each(double (*f) (double), nialptr x)
{
  nialptr     z;
  nialint     i,
              tx = tally(x);
  double     *xptr,
             *zptr;
  int         v = valence(x);
  int         usex = refcnt(x) == 0;


  if (usex)  /* x is a temporary, we can overwrite its items */
    z = x;
  else   /* create the result container */
    z = new_create_array(realtype, v, 0, shpptr(x, v));

  /* set up pointers and loop over the items applying f */
  xptr = pfirstreal(x);      /* safe */
  zptr = pfirstreal(z);      /* safe */
  for (i = 0; i < tx; i++)
    *zptr++ = (*f) (*xptr++);

  apush(z);
  if (!usex)
    freeup(x);
}

/* routine that implements the transformer EACHLEFT. 
   It handles some primitive cases directly otherwise it
   uses the identity
      A EACHLEFT f B = A EACHBOTH f (single B)
*/

void
ieachleft()
{
  nialptr     f,
              x,
              y,
              z;

  f = apop();  /* the functional arg */

  /* test if top is a fault other than the special ones */
  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;

  z = apop();  /* the data argument, check that it is a pair */
  if (tally(z) != 2) {
    apush(makefault("?argument to an EACHLEFT transform must be a pair"));
  /* freeup done below */
  }

  else { /* arg is a pair, split it into x and y */
    int         prop = get_prop(f);

    splitfb(z, &x, &y);

    if (!atomic(y)) {
      apush(y);              /* achieve eachleft by singling the right argument 
                                and using eachboth */
      isingle();
      y = apop();
    }
    
    /* handle some basic operations specially */
    if (tag(f) == t_basic && (prop == 'B' || prop == 'C' || prop == 'R'))  
    { 
      if (prop == 'C' || prop == 'R')
        /* primitive is binary pervasive or multi pervasive directly,
           use the version that assumes a C routine */

      { apush(x);
        apush(y);
        APPLYBINARYPRIM(f);
      }

      else /* f has a binary version, apply C routine directly */
      { void  (*fn) (void);
        fn = binapplytab[get_binindex(f)];
        int_eachboth(fn, x, y); /* use internal version of eachboth */
      }
    }
    else  /* use eachboth */
      eachboth(f, x, y);
  }

#ifdef FP_EXCEPTION_FLAG
  fp_checksignal();
#endif

  freeup(z);
}


/* routine that implements EACHRIGHT. 
   It handles some primitive cases directly otherwise it
   uses the identity
      A EACHRIGHT f B = (single A) EACHBOTH f B
*/

void
ieachright()
{
  nialptr     f,
              x,
              y,
              z;

  f = apop(); /* the functional arg */

  /* test if top is a fault other than the special ones */
  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;

  z = apop(); /* the data argument, check that it is a pair */
  if (tally(z) != 2) {
    apush(makefault("?argument to an EACHRIGHT transform must be a pair"));
         /* freeup done below */
  }

  else { /* arg is a pair, split it into x and y */

    int         prop = get_prop(f);

    splitfb(z, &x, &y);
    if (!atomic(x)) {
      apush(x);              /* achieve eachleft by singling the right
                              * argument and using eachboth */
      isingle();
      x = apop();
    }

   /* handle some basic operations specially */
    if (tag(f) == t_basic && (prop == 'B' || prop == 'C' || prop == 'R'))
    { 
       if (prop == 'C' || prop == 'R')
           /* primitive is binary pervasive or multi pervasive directly,
           use the version that assumes a C routine */
      { apush(x);
        apush(y);
        APPLYBINARYPRIM(f);
      }

      else /* f has a binary version, apply C routine directly */
      { void  (*fn) (void);
        fn = binapplytab[get_binindex(f)];
        int_eachboth(fn, x, y);  /* use internal version of eachboth */
      }
    }
    else /* use eachboth */
      eachboth(f, x, y);
  }
#ifdef FP_EXCEPTION_FLAG
  fp_checksignal();
#endif
  freeup(z);
}


/* routine to implement the primitive OUTER using the identity
       OUTER f A = EACH f cart A
   It optimizes the case where f is a basic operation and A
   has two items.
*/

void
iouter()
{
  nialptr     f;

  f = apop();  /* get the functional argument */

  /* test for case to optimize */
  if (tag(f) == t_basic && tally(top) == 2) 
  { nialptr     a,
                b,
                c = apop(),  /* the data argument */
                z,
                shz;
    nialptr     res,
                ai,
                bj;
    int         kz,
                va,
                vb,
                binflag,
                prop;
    nialint     vz,
      ta,
                tb,
                i,
                j,
                k,
                tz;

    /* split the pair into a and b */
    splitfb(c, &a, &b);
    va = valence(a);
    vb = valence(b);

    /* compute the valence of the result */
    vz = va + vb;  

   /* compute the shape of the result */
    shz = new_create_array(inttype, 1, 0, &vz);
    for (i = 0; i < va; i++)
      store_int(shz, i, pickshape(a, i));
    for (j = 0; j < vb; j++) 
    {  store_int(shz, i, pickshape(b, j));
      i++;
    }

    /* compute the tally of the result and check it is not too large */
    if (!prodints(pfirstint(shz), vz, &tz)) {
      freeup(shz);
      apush(makefault("?result size too large in OUTER transform"));
      freeup(c);
      return;
    }

    if (tz == 0) {           /* the result is empty */
      z = new_create_array(atype, vz, 0, pfirstint(shz));
      apush(z);
      freeup(shz);
      freeup(c);    /* since z is empty, c must be a pair of empties and
                       hence of kind atype, therefore a and b will be freed */
      return;
    }
    
    /* non-empty result */

    /* compute first result item get expected result type */
    pair(fetchasarray(a, 0), fetchasarray(b, 0));
    APPLYPRIMITIVE(f);

#ifdef FP_EXCEPTION_FLAG
    fp_checksignal();
#endif
   
    res = apop();
    kz = kind(res);
    kz = atomic(res) && homotype(kz) ? kz : atype;

    /* create the result container */
    z = new_create_array(kz, vz, 0, pfirstint(shz));
    freeup(shz);

    /* store first item */
    if (kz == atype) {
      store_array(z, 0, res);/* block is necessary */
    }
    else {
      copy1(z, 0, res, 0);
      freeup(res);
    }

    ta = tally(a);
    tb = tally(b);

    /* set flag if C routine has a binary version */
    prop = get_prop(f);
    binflag = (prop == 'B' || prop == 'C' || prop == 'R');

    k = 0;     /* the counter for storing items of z */

    /* loop over the items of a */
    for (i = 0; i < ta; i++) {
      ai = fetchasarray(a, i);
      apush(ai);             /* to protect it for several uses */

      /* loop over the items of b */
      for (j = 0; j < tb; j++) {
        if (k == 0)          /* no need to recompute first item */
          k++;
        else { /* apply appropriate call for primitive property */
          bj = fetchasarray(b, j);
          if (binflag)
          { apush(ai);
            apush(bj);
            APPLYBINARYPRIM(f);
          }
          else
          { pair(ai, bj);
            APPLYPRIMITIVE(f);
          }
#ifdef FP_EXCEPTION_FLAG
          fp_checksignal();
#endif
          res = apop();

          /* if type is consistent store the result */
          if (atomic(res) && kind(res) == kz) {
            copy1(z, k, res, 0);
            freeup(res);
          }
          else {
            if (kz == atype) {
              store_array(z, k, res); /* block is necessary for store_array macro */
            }

            else { /* blow up result container since kind has changed */
              z = explode(z, vz, tz, 0, k);
              store_array(z, k, res);
              kz = atype;
            }
          }
          k++;
        }  
      } /* end of inner loop */
      freeup(apop());        /* to unprotect ai */
    } /* end of outer loop */

    apush(z);
    freeup(a);
    freeup(b);               /* needed if c is homotype */
    freeup(c);
  }
  else /* use the identity */
  { icart();
    each(f, apop());
  }
}


/* routine to implement the transformer LEAF.
   Uses the definition
      LEAF f A = IF atomic A THEN f A ELSE EACH (LEAF f) A ENDIF
*/

void
ileaf()
{
  nialptr     f, g, x;
  f = apop();
  if (atomic(top))   /* apply f to the atom */
    do_apply(f);

  else  /* prepare to do each (leaf f)*/
  { x = apop();
    g = b_transform(leafcode, f);
    each(g, x);
    apush(f);                /* to protect f as component of g */
    freeup(g);
    apop();                  /* unprotect f */
  }
}

/* implements the transformer TWIG
  Uses the equation
     TWIG f A = IF simple A THEN f A ELSE EACH (TWIG f) A ENDIF
*/

void
itwig()
{
  nialptr     f, g, x;
  f = apop();
  if (simple(top)) /* apply f to the simple array */
    do_apply(f);

  else { /* prepare to do each (twig f) */
    x = apop();
    g = b_transform(twigcode, f);
    each(g, x);
    apush(f);                /* to protect f as component of g */
    freeup(g);
    apop();                  /* unprotect f */
  }
}


/* If is is a multipervasive basic operation then apply it directly,
   otherwise use the formal definition EACHALL f A = EACH f pack A. 
*/

void
ieachall()
{
  if (tag(top) == t_basic && get_prop(top) == 'R') {  /* f is multipervasive */
    nialptr     f = apop();

    APPLYPRIMITIVE(f);

#ifdef FP_EXCEPTION_FLAG
    fp_checksignal();
#endif

  }
  else {   /* apply the identity */
    swap();    /* to put data arg on top */
    ipack();
    swap();   /* to put fun arg back on top */
    ieach();
  }
}

/* routine to implement the transformer EACHBOTH */

void
ieachboth()
{
  nialptr     f = apop();

    /* test if top is a fault other than the special ones */
  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;

  /* test that arg is a pair, otherwise error */
  if (tally(top) != 2) {
    freeup(apop());          /* remove arg */
    apush(makefault("?argument to an EACHBOTH transform must be a pair"));
  }

  else { /* arg is a pair, split it into x and y */
    nialptr     x,
                y,
                z = apop();
    int         prop = get_prop(f);

    splitfb(z, &x, &y);

    /* test whether f is a basic operation with a binary version */
    if (tag(f) == t_basic && (prop == 'B' || prop == 'C' || prop == 'R'))

    {
      if (prop == 'C') { 
        /* primitive is binary pervasive directly, apply it */
        apush(x);
        apush(y);
        APPLYBINARYPRIM(f);
      }
      else  /* use the version of eachboth that applies a C function */
      { void        (*fn) (void);
        fn = binapplytab[get_binindex(f)];
        int_eachboth(fn, x, y);
      }
    }
    else /* use the version of eachboth that assumes f is parse tree */
      eachboth(f, x, y);
#ifdef FP_EXCEPTION_FLAG
    fp_checksignal();
#endif
    freeup(z);
  }
}

/* The tranformer EACH is defined by: A EACHBOTH f B = EACHALL f (A B).
   This routine implements it efficiently by avoiding the "pack" implicit in
   EACHALL.

   An argument is replicated if it has valence 0.

   If one of the arguments is not replicated and they are
   of different shape then the result is the fault ?conform.
*/

static void
eachboth(nialptr f, nialptr x, nialptr y)
{
  int         left,
              right,
              v;
  nialint     i,
              t,
             *shp;
  nialptr     z,
              xi = Null,
              yi = Null;


  left = tally(x) == 1;

  if (left) {                /* set up eachright */
    if (atomic(x))
      xi = x;
    else
      xi = fetchasarray(x, 0);
    apush(xi);               /* to hold xi during multiple uses */
  }

  right = tally(y) == 1;

  if (right) {               /* set up eachleft */
    if (atomic(y))
      yi = y;
    else
      yi = fetchasarray(y, 0);
    apush(yi);               /* to hold yi during multiple uses */
  }

  /* return a fault if they do not conform in shape */
  if (!left && !right && !equalshape(x, y)) {
    apush(makefault("?conform"));
    freeup(x);
    freeup(y);
    return;
  }

  /* create the result container and fill it */
  t = left ? tally(y) : tally(x);

  if (left && right) {       /* choose the one with higher valence */
    v = (valence(x) > valence(y) ? valence(x) : valence(y));
    shp = (valence(x) > valence(y) ? shpptr(x, v) : shpptr(y, v));
  }
  else {
    v = left ? valence(y) : valence(x);
    shp = (left ? shpptr(y, v) : shpptr(x, v));
  }
  /* assignments to shp later in the code are safe since 
     it is only used in next create */
  z = new_create_array(atype, v, 0, shp);

  /* loop to pick up the items, apply f and store the result */
  for (i = 0; i < t; i++) {
    if (!left)
      xi = fetchasarray(x, i);
    if (!right)
      yi = fetchasarray(y, i);
    pair(xi, yi);
    do_apply(f);
    store_array(z, i, apop());

#ifdef USER_BREAK_FLAG
    checksignal(NC_CS_NORMAL);
#endif
  }

  /* if the result is a single holding an atom, return the atom */
  if (v == 0 && atomic(fetch_array(z, 0))) {
    apush(fetch_array(z, 0));
    freeup(z);
  }
  else { /* test if the result can be homogeneous */
    if (homotest(z))
      z = implode(z);
    apush(z);
  }
  /* freeup the data arguments */
  freeup(x);
  freeup(y);

  /* clean up saved item used in replication if there is one */
  if (right) {
    swap();
    freeup(apop());
  }
  if (left) {
    swap();
    freeup(apop());
  }
}

/* routine to do eachboth on a primitive binary operation. The structure
   of the routine is similar to the one above. Used in the
   pervading operations.
*/

void
int_eachboth(void (*f) (void), nialptr x, nialptr y)
{
  int         left,
              right,
              v;
  nialint     i,
              t,
             *shp;
  nialptr     z,
              xi = Null,
              yi = Null;

  left = tally(x) == 1;
  if (left) {                /* set up eachright */
    if (atomic(x))
      xi = x;
    else
      xi = fetchasarray(x, 0);
    apush(xi);               /* to hold xi during multiple uses */
  }

  right = tally(y) == 1;

  if (right) {               /* set up eachleft */
    if (atomic(y))
      yi = y;
    else
      yi = fetchasarray(y, 0);
    apush(yi);               /* to hold yi during multiple uses */
  }

  /* return a fault if they do not conform in shape */
  if (!left && !right && !equalshape(x, y)) {
    apush(makefault("?conform"));
    freeup(x);
    freeup(y);
    return;
  }

  /* create the result container and fill it */
  t = left ? tally(y) : tally(x);

  if (left && right) {       /* choose the one with higher valence */
    v = (valence(x) > valence(y) ? valence(x) : valence(y));
    shp = (valence(x) > valence(y) ? shpptr(x, v) : shpptr(y, v));
  }
  else {
    v = left ? valence(y) : valence(x);
    shp = (left ? shpptr(y, v) : shpptr(x, v));
  }

  /* assignments to shp in the code below are safe since 
     it is only used in next create */
  z = new_create_array(atype, v, 0, shp);

  /* loop to pick up the items, apply f and store the result */
  for (i = 0; i < t; i++) {
    if (!left)
      xi = fetchasarray(x, i);
    if (!right)
      yi = fetchasarray(y, i);
    apush(xi);
    apush(yi);
    (*f) ();
    store_array(z, i, apop());

#ifdef USER_BREAK_FLAG
    checksignal(NC_CS_NORMAL);
#endif

  }

/* if the result is a single holding an atom, return the atom */
  if (v == 0 && atomic(fetch_array(z, 0))) {
    apush(fetch_array(z, 0));
    freeup(z);
  }

  else { /* test if the result can be homogeneous */
    if (homotest(z))
      z = implode(z);
    apush(z);
  }

  /* clean up the data arguments */
  freeup(x);
  freeup(y);

  /* clean up an item saved for replication if there is one */
  if (right) {
    swap();
    freeup(apop());
  }
  if (left) {
    swap();
    freeup(apop());
  }
}



/* implements the transformer FOLD as
    n FOLD f x   ==   f f f...f x  (n applications) */

void
ifold()
{
  nialptr     f,
              z,
              x,
              y;
  nialint     n,
              i;

  f = apop();

  /* if top is one of the special faults return it */
  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;

  z = apop();

  /* test that the arg is a pair */
  if (tally(z) != 2) {
    apush(makefault("?argument to a FOLD transform must be a pair"));
  }

  else { /* aplit the pair into x and y */
    splitfb(z, &x, &y);
   
    /* test that the left argument is an atomic integer */
    if (kind(x) != inttype || valence(x) != 0) {
      apush(makefault("?first argument of FOLD must be an integer"));
      freeup(x);
      freeup(y);             /* in case they are temporary */
    }

    else { /* get the integer argument */
      n = intval(x);
      freeup(x);  /* in case x is temporary */

      /* push y and apply f n times */
      apush(y);
      for (i = 0; i < n; i++)
        do_apply(f);
    }
  }
  freeup(z);
}

/* implements the primitive transformer CONVERSE, where
        A CONVERSE f B = B f A 
*/

void
iconverse()
{
  nialptr     f = apop();

  /* test that the argument is a pair */
  if (tally(top) != 2) {
    freeup(apop());
    apush(makefault("?argument to a CONVERSE transform must be a pair"));
  }
  else { /* use the definition */
    ireverse();
    do_apply(f);
  }
}



/* routines to implement the sorting transformers SORT and GRADE using an internal
   routine that does the work for both primitives. */

void
isort()
{
  nialptr     f,
              x;

  f = apop();
  x = apop();
  sort(f, x, false);
}

void
igrade()
{
  nialptr     f,
              x;
  f = apop();
  x = apop();
  sort(f, x, true);

}


/* The following routine compute sort f x or grade f x, for a comparator f, where
   gradesw=1 implies grade f x, otherwise sort f x.

   The routine uses 2 different approaches depending on the type of the
   data argument: 
      If x is an integer array and the comparator is <= or "up" then a radix sort 
      is done.

      Otherwise a merge sort from Knuth's book is used. It is described
      by the implmenter Jean Michel as follows:
        List merge sort. Algorithm 5.2.4-L of Knuth Vol. 3 improved according
        to exercise 12 (but not exercise 15). Exercise 12 uses initial runs
        to speed up the algorithm and hence if x is already sorted it does
        minimal work.

       Quoting from Knuth (who speaks in origin 1):

       "after sorting is complete, l[0] is the index of the smallest element
       of x ;and for 1<=k<=n, l[k] is the index of the element of x which
       follows x[k], or l[k]=0 if x[k] is the greatest element of x. "

       I have used the meaningless variable names L,s,t,p,q on purpose so
       the algorithm below can be easily matched against Knuth's text.
       I just removed the goto's from Knuth's algorithm, using while loops.
       His variable K[i] is replaced by x[i-1].

       "comments": Insert sort is faster for small values of n. 
       Distribution sort is faster for discrete types (characters, booleans). 
       The algorithm uses extra space (an integer array l of length n+2) . 
   
   A radix sort for real arrays was tested but did not improve 
   performance significantly.

   The sort routine is used in several of the primitives array operations
   to achieve N (log N) performance rather than N * N . The array representation
   includes a flag to indicated whether an array is sorted according to the "up"
   comparator.
*/
void
sort(nialptr f, nialptr x, int gradesw)
{
  nialptr     l,
              m,
              val;
  nialint     n,
              t,
              p,
              q,
              s,
             *L;
  int         v,
              kx;
  int         lteflag,
              gteflag,
              speedup,
              tv = false;
  if (atomic(x)) {
    if (gradesw) {
      apush(Null);
      isingle();
    }
    else
      apush(x);
  }
  else {
    nialint     n2;

    kx = kind(x);
    if ((is_sorted(x) || check_sorted(x)) &&
          (f == upcode || (f == ltecode && (kx == inttype || kx == realtype))))
    { /* array is already sorted */
      if (gradesw)
      { /* build the grade result */  
        apush(x);
        ishape();
        igage();
        itell();
      }

      else /* result of sort is the argument */
        apush(x);

      return;
    }

    lteflag = (f == upcode || (f == ltecode && simple(x)));
    gteflag = f == gtecode && homotype(kx);
    speedup = lteflag || gteflag;
    if (homotype(kx) && !speedup) {
      x = explode(x, valence(x), tally(x), 0, tally(x));
      kx = atype;
    }

    /* start of algorithm from Knuth */
    n = tally(x);
    n2 = n + 2;

    /* create the temporary array with 2 extra slots */
    l = new_create_array(inttype, 1, 0, &n2);

    L = pfirstint(l);        /* safe: reloaded below */
    L[0] = 1;
    t = n + 1;
    if (n >= 2) {
      /* loop over pairs of adjacent items comparing them */
      for (p = 1; p < n; p++) {
        if (lteflag) { /* use C comparator on atomic types, "up" on arrays */
          switch (kx) {
            case inttype:
                tv = fetch_int(x, p - 1) <= fetch_int(x, p);
                break;
            case realtype:
                tv = fetch_real(x, p - 1) <= fetch_real(x, p);
                break;
            case chartype:
                tv = invseq[fetch_char(x, p - 1) - LOWCHAR] <=
                  invseq[fetch_char(x, p) - LOWCHAR];
                break;
            case booltype:
                {
                  int         it = fetch_bool(x, p - 1);

                  tv = it <= fetch_bool(x, p);
                }
                break;
            case atype:
                {
                  int         it = fetch_array(x, p - 1),
                              it1 = fetch_array(x, p);

                  tv = up(it, it1);
                  break;
                }
          }
        }
        else if (gteflag) { /* use C comparator on atomic types, "up" on arrays */

          switch (kx) {
            case inttype:
                tv = fetch_int(x, p - 1) >= fetch_int(x, p);
                break;
            case realtype:
                tv = fetch_real(x, p - 1) >= fetch_real(x, p);
                break;
            case chartype:
                tv = invseq[fetch_char(x, p - 1) - LOWCHAR] >=
                  invseq[fetch_char(x, p) - LOWCHAR];
                break;
            case booltype:
                {
                  int         it = fetch_bool(x, p - 1);

                  tv = it >= fetch_bool(x, p);
                }
                break;
          }
        }
        else { /* apply the conparator that was provided */
          pair(fetch_array(x, p - 1), fetch_array(x, p));
          do_apply(f);
          val = apop();
          if (kind(val) != booltype || valence(val) != 0) {
            freeup(val);
            goto badcompare;
          }
          tv = boolval(val);
        }
        /* if pair is in order, store positive index in L, else store negative one */
        L = pfirstint(l);    /* safe: reloaded here */
        if (tv)
          L[p] = p + 1;
        else {
          L[t] = -(p + 1);
          t = p;
        }
      } /* end of loop comparing adjacent items */

      L[t] = 0;
      L[n] = 0;
      L[n + 1] = -L[n + 1];
      /* loop indefinitely util L[n+1] == 0 */
      while (L[n + 1] != 0) {
        s = 0;
        t = n + 1;
        q = L[t];
        p = L[s];
       
        /* loop indefinitely comparing items until q == 0 */
        do { /* compute comparison between p-1 and q-1 items 
                using the same strategy as above */
          if (lteflag) {
            switch (kx) {
              case inttype:
                  tv = fetch_int(x, p - 1) <= fetch_int(x, q - 1);
                  break;
              case realtype:
                  tv = fetch_real(x, p - 1) <= fetch_real(x, q - 1);
                  break;
              case chartype:
                  tv = invseq[fetch_char(x, p - 1) - LOWCHAR] <=
                    invseq[fetch_char(x, q - 1) - LOWCHAR];
                  break;
              case booltype:
                  {
                    int         it = fetch_bool(x, p - 1);

                    tv = it <= fetch_bool(x, q - 1);
                  }
                  break;
              case atype:
                  {
                    int         it = fetch_array(x, p - 1),
                                it1 = fetch_array(x, q - 1);

                    tv = up(it, it1);
                  }
                  break;
            }
          }
          else if (gteflag) {
            switch (kx) {
              case inttype:
                  tv = fetch_int(x, p - 1) >= fetch_int(x, q - 1);
                  break;
              case realtype:
                  tv = fetch_real(x, p - 1) >= fetch_real(x, q - 1);
                  break;
              case chartype:
                  tv = invseq[fetch_char(x, p - 1) - LOWCHAR] >=
                    invseq[fetch_char(x, q - 1) - LOWCHAR];
                  break;
              case booltype:
                  {
                    int         it = fetch_bool(x, p - 1);

                    tv = it >= fetch_bool(x, q - 1);
                  }
                  break;
            }
          }
          else {
            pair(fetch_array(x, p - 1), fetch_array(x, q - 1));
            do_apply(f);
            val = apop();
            if (kind(val) != booltype || valence(val) != 0) {
              freeup(val);
              goto badcompare;
            }
            tv = boolval(val);
          }
          /* use the comparison result to modify pointers in L */
          L = pfirstint(l);  /* safe: reloaded here */
          if (tv) {
            L[s] = L[s] < 0 ? -p : p;
            s = p;
            p = L[p];
            if (p > 0)
              continue;
            L[s] = q;
            s = t;
            do {
              t = q;
              q = L[q];
            } while (q > 0);
          }
          else {
            L[s] = L[s] < 0 ? -q : q;
            s = q;
            q = L[q];
            if (q > 0)
              continue;
            L[s] = p;
            s = t;
            do {
              t = p;
              p = L[p];
            } while (p > 0);
          }
          p = -p;
          q = -q;
        } while (q != 0);  /* end of inner loop */
        L[s] = L[s] < 0 ? -p : p;
        L[t] = 0;

#ifdef USER_BREAK_FLAG
        checksignal(NC_CS_NORMAL);
#endif

      }  /* end of outer loop */
    }
/* second part: reconstitute the result (grade or sort) following the
                links in l  */

    v = valence(x);
    t = 0;
    if (gradesw) { 
    /* store the addresses to achieve the sort using choose */
      m = new_create_array(v == 1 ? inttype : atype, v, 0, shpptr(x, v));
      L = pfirstint(l);      /* safe : reestablish L in case heap moved */
      for (s = 0; s < n; s++) {
        t = L[t];
        if (v == 1)
          store_int(m, s, t - 1);
        else
          store_array(m, s, ToAddress(t - 1, shpptr(x, v), v));
      }

    }

    else { /* create the result array and copy items into it */
      m = new_create_array(kx, v, 0, shpptr(x, v));
      L = pfirstint(l);      /* reestablish L in case heap moved */
      for (s = 0; s < n; s++) {
        t = L[t];
        copy1(m, s, x, t - 1);
      }

      if (homotest(m))  /* test if the result is homogeneous */
        m = implode(m);
      if (f == upcode || (f == ltecode && (kx == inttype || kx == realtype)))
      {
        set_sorted(m, true); /* record that the result is sorted by "up" */
      }
    }
    freeup(l);
    apush(m);

  }
  freeup(x);

  return;
badcompare:
  freeup(l);
  freeup(x);
  if (gradesw) {
    apush(makefault("?invalid comparison in GRADE"));
  }
  else
    apush(makefault("?invalid comparison in SORT"));
}


/* linear sorting algorithm based on radix sorting.
   Routine provided by Henk Meijer.
*/





/* routine to implement the transformer FORK with 3 or more operations
  FORK [f,g,h,k,...,z] A =
       IF f A THEN
          g A
       ELSEIF h A THEN
          k A
       ELSEIF
         ...
       ELSE
          z A
       ENDIF

  The result is Nullexpr if there is no else operation
*/

void
ifork()
{
  nialptr     f,
              closure = Null;
  int         closureflag = false;

#ifdef PROFILE
  if (triggered || profile)  /* the argument has not been coerced */
#   else
  if (triggered)             /* the argument has not been coerced */
#   endif
  {
    if (tag(top) == t_closure) {  /* if a closure was done then it must be
                                     redone after the coercion so that an
                                     atlas won't be hidden. */
      nialptr     op = apop();

      apush(get_op(op));
      coerceop();
      closureflag = b_closure();
      if (closureflag) {
        incrrefcnt(top);     /* to protect closure */
        closure = top;
      }
      freeup(op);
    }
    else
      coerceop();
  }
  f = apop();
  if (tag(f) != t_atlas) {
    freeup(apop());
    buildfault("arg to fork must be an atlas");
  }
  else if (tally(f) < 3) {
    freeup(apop());
    buildfault("atlas to fork must have two or more operations");
  }
  else {
    nialptr     z;
    int         i = 1,
                done = false;

    while (!done && i < tally(f) - 1) {
      apush(top);            /* copy arg to use for test */
      do_apply(fetch_array(f, i)); /* if test. */
      z = apop();
      if (kind(z) == booltype && valence(z) == 0) {
        if (boolval(z)) {
          done = true;
          do_apply(fetch_array(f, i + 1)); /* uses copy of arg */
        }
        i += 2;              /* skip the next operation */
      }
      else {
        freeup(apop());      /* get rid of the copy of the arg */
        apush(Logical);      /* result is Logical fault */
        done = true;
      }
      freeup(z);             /* test value is temporary */
    }
    if (!done)               /* do else case */
    { if (i == tally(f))     /* even number of operations */
      { freeup(apop());      /* remove copy of argument */
        apush(Nullexpr);     /* result is ?noexpr */
      }
      else /* result is computed from last operation */
        do_apply(fetch_array(f, i)); /* uses copy of arg */
    }
  }
  if (closureflag) {
    decrrefcnt(closure);
    freeup(closure);
  }
}

 
/*  tail recursion transformer based on loop implementation

Formal definition is:

RECUR is TR test endf parta joinf partb
   (FORK [test,endf,joinf[parta, RECUR [test,endf,parta,joinf,partb] partb]])

Implemented as :

RECUR is TR test endf parta joinf partb OP A {
    Elements := Null;
    WHILE not test A DO
      Elements := Elements append parta A;
      A := partb A;
    ENDWHILE;
    Res := endf A;
    FOR E WITH reverse Elements DO
      Res := E joinf Res;
    ENDFOR;
    Res }
*/

void
irecur()
{
  nialptr     f,
              closure = Null;
  int         closureflag = false;

#ifdef PROFILE
  if (triggered || profile)  /* the argument has not been coerced */
#   else
  if (triggered)             /* the argument has not been coerced */
#   endif
  {
    if (tag(top) == t_closure) {  /* if a closure was done then it must be
                                     redone after the coercion so that an
                                     atlas won't be hidden. */
      nialptr     op = apop();

      apush(get_op(op));
      coerceop();
      closureflag = b_closure();
      if (closureflag) {
        incrrefcnt(top);     /* to protect closure */
        closure = top;
      }
      freeup(op);
    }
    else
      coerceop();
  }

  f = apop();
  if (tag(f) != t_atlas) {
    freeup(apop());
    buildfault("arg to RECUR must be an atlas");
  }

  else 
  if (tally(f) != 6) {
    freeup(apop());
    buildfault("atlas to RECUR must have five operations");
  }

  else { /* get the parts of the atlas */
    nialptr test, endf, parta, partb, joinf, elements, res;
    nialint size = 1000, incr = 1000, i = 0, cont;
    test = fetch_array(f,1);
    endf = fetch_array(f,2);
    parta = fetch_array(f,3);
    joinf = fetch_array(f,4);
    partb = fetch_array(f,5);

    /* create array to hold partial values. This is created of size
       1000 and increased by that amount if needed */

    elements = new_create_array(atype,1,0,&size);

    apush(top);  /* protect argument */

   /* apply the test function to the argument and check it is boolean */
    do_apply(test); 
    res = apop();
    if (kind(res)==booltype)
    { cont = boolval(res)==0;  /* set continue flag if false */
      freeup(res);
    }
    else
    { freeup(apop());
      freeup(res);
      buildfault("invalid test in RECUR");
      goto cleanup;
    }
    freeup(res);

    /* loop while the test function result remains false */
    while (cont)
    { apush(top);
      
      do_apply(parta);  /* apply the parta function to the arg */

      if (i==size)  /* need to expand elements array */
      { nialptr temp;
        size = size + incr;
        temp = new_create_array(atype,1,0,&size);
        copy(temp,0,elements,0,size - incr);
        freeup(elements);
        elements = temp;
      }

      store_array(elements,i,apop()); /* store the parta result */
      i++;

      do_apply(partb); /* apply the partb function to the result so far */

      apush(top);
      do_apply(test); /* apply the test to the arg */
      res = apop();
      if (kind(res)==booltype)
      { cont = boolval(res)==0; /* set continue flag if false */

        freeup(res);
      }
      else
      { freeup(apop());
        freeup(res);
        buildfault("invalid test in RECUR");
        goto cleanup;
      }
    }  /* end of loop */

    do_apply(endf); /* apply the endf to the result so far */

    i--;

    /* loop apply the joinf function to the pair formed from
       an element item and the result so far */
    while (i>=0)
    { 
      if (tag(joinf)==t_basic && get_prop(joinf)=='B')
      { /* use the binary primitive directly */
        apush(fetch_array(elements,i));
        swap();
        (*binapplytab[get_binindex(joinf)])();
      }

      else /* create the pair and apply joinf */
      { pair(fetch_array(elements,i),apop());
        do_apply(joinf);
      }
      i--;
    }
    /* end of loop, result is on the stack */
  cleanup:
    freeup(elements);
  }
  if (closureflag) {
    decrrefcnt(closure);
    freeup(closure);
  }
}


/* routine to implement the Nial transformer to do length recursion 
   based on first and rest. The formal definition using RECUR is:

     ACROSS IS TRANSFORMER endf parta joinf
   ( RECUR [empty, endf, parta first, joinf, rest ] list )

   The following loop implementation is more efficient and is used in
   the C code below:

   ACROSS IS TR endf parta joinf OP A {
      Res := endf void last A;
      FOR I WITH reverse tell tally A DO
        Res := parta A@I joinf Res
      ENDFOR }

*/

void
iacross()
{
  nialptr     f,
              closure = Null;
  int         closureflag = false;

#ifdef PROFILE
  if (triggered || profile)  /* the argument has not been coerced */
#   else
  if (triggered)             /* the argument has not been coerced */
#   endif
  {
    if (tag(top) == t_closure) {  /* if a closure was done then it must be
                                     redone after the coercion so that an
                                     atlas won't be hidden. */
      nialptr     op = apop();

      apush(get_op(op));
      coerceop();
      closureflag = b_closure();
      if (closureflag) {
        incrrefcnt(top);     /* to protect closure */
        closure = top;
      }
      freeup(op);
    }
    else
      coerceop();
  }

  /* get the function arg and validate it */
  f = apop();
  if (tag(f) != t_atlas) {
    freeup(apop());
    buildfault("arg to ACROSS must be an atlas");
  }
  else if (tally(f) != 4) {
    freeup(apop());
    buildfault("atlas to ACROSS must have three operations");
  }

  else { /* get the function arguments from the atlas */
    nialptr a, endf, parta, joinf, parta_res;
    nialint i;
    endf = fetch_array(f,1);
    parta = fetch_array(f,2);
    joinf = fetch_array(f,3);
    apush(top);  /* to protect it */
    a = apop();

    /* set up the end value and apply the endf function */

    apush(Null);  /* end value */
    do_apply(endf);

    /* loop over items of the argument in reverse order */
    for (i=tally(a) - 1;i>=0;i--)
    { /* apply parta to the ith item */
      apush(fetchasarray(a,i));
      do_apply(parta);

      /* apply joinf to the pair formed by the parta result
        and the result so far */
      if (tag(joinf)==t_basic && get_prop(joinf)=='B')
      { /* use the binary primitive directly */
        swap();
        (*binapplytab[get_binindex(joinf)])();
      }
      else /* form the pair and  apply joinf */
      { parta_res = apop();
        pair(parta_res,apop());
        do_apply(joinf);
      }
    } /* end of loop, result is on top with copy of arg below */

    swap();
    freeup(apop()); /* unprotect and free a */
  }

  if (closureflag) {
    decrrefcnt(closure);
    freeup(closure);
  }
}


/* routine to implement the transformer DOWN that does depth recursion.
   The formal definition using RECUR is:

      DOWN IS TRANSFORMER test endf structf joinf 
    	( FORK [test, endf, joinf EACH DOWN [test, endf, structf, joinf] structf ] )
 
   The following loop implementation is more efficient and is used in
   the C code below:

   DOWN IS TR test endf structf joinf OP A {
      Candidates := [A];
      Results := Null;
      WHILE not empty Candidates DO
        Candidates B := [front,last] Candidates;
        IF B = "MARKER THEN
           Candidates Shp := [front,last] Candidates;
           N := prod Shp;
           Results Items := opp N [drop,take] Results;
           Results := Results append joinf (Shp reshape Items);   
        ELSEIF test B THEN
           Results := results append endf B;
        ELSE
           B := structf B;
           Candidates := Candidates link [shape B,"MARKER] link reverse list B;
        ENDIF;
      ENDWHILE;
      first Results }

*/

void
idown()
{
  nialptr     f,
              closure = Null;
  int         closureflag = false;

#ifdef PROFILE
  if (triggered || profile)  /* the argument has not been coerced */
#   else
  if (triggered)             /* the argument has not been coerced */
#   endif
  {
    if (tag(top) == t_closure) {  /* if a closure was done then it must be
                                     redone after the coercion so that an
                                     atlas won't be hidden. */
      nialptr     op = apop();

      apush(get_op(op));
      coerceop();
      closureflag = b_closure();
      if (closureflag) {
        incrrefcnt(top);     /* to protect closure */
        closure = top;
      }
      freeup(op);
    }
    else
      coerceop();
  }

  /* get the functional argument and validate it */
  f = apop();
  if (tag(f) != t_atlas) {
    freeup(apop());
    buildfault("arg to DOWN must be an atlas");
  }

  else 
   if (tally(f) != 5) {
    freeup(apop());
    buildfault("atlas to DOWN must have four operations");
  }

  else { /* get the functional items of the atlas */
    nialptr test, endf, structf, joinf, candidates, 
      results, shp, marker, b, res;
    nialint isize = 1000, jsize = 1000, incr = 1000, i, j, k, succ, n;
    test = fetch_array(f,1);
    endf = fetch_array(f,2);
    structf = fetch_array(f,3);
    joinf = fetch_array(f,4);

    /* set up temporary arrays and a marker array */
    candidates = new_create_array(atype,1,0,&isize);
    results = new_create_array(atype,1,0,&jsize);
    marker = makephrase(" _ )marker [_ ");   /* chosen to avoid conflicts */
    incrrefcnt(marker);

    /* initialize candidates array */
    store_array(candidates,0,apop());

    /* loop over candidates */
    i = 1;
    j = 0;
    while (i>0)
    { i--;


      /* get the next candidate and clear its slot */
      b = fetch_array(candidates,i);
      decrrefcnt(b);
      store_int(candidates,i,invalidptr); /* to clear the item slot */

      if (b==marker)
      { /* item before is the shape, compute the number of items */
        i--;
        shp = fetch_array(candidates,i);
        decrrefcnt(shp);
        store_int(candidates,i,invalidptr); /* to clear the item slot */
        apush(shp); /* for use in reshape below */
        apush(top);
        iproduct();
        res = apop();
        n = intval(res);
        freeup(res);

        /* loop over the last n items of the results pushing them */
        for (k=0;k<n;k++)
        { 
          apush(fetch_array(results,j-n+k));
          decrrefcnt(top);
          store_int(results,j-n+k,invalidptr); /* to clear the item slot */
        } 
        j = j - n;
        /* make a list and reshape it */
        mklist(n);
        b_reshape();

        /* use function joinf on the  the reshape result before appending it 
           to the results array, which is expanded if necesary */
        do_apply(joinf);
        if (j==jsize)
        { nialptr temp;
          jsize = jsize + incr;
          temp = new_create_array(atype,1,0,&jsize);
          copy(temp,0,results,0,j);
          freeup(results);
          results = temp;
        }
        store_array(results,j,apop());
        j++;
      }

      else  
      /* not a marker, test b with endf */
      { apush(b);
        apush(top);
        do_apply(test);
        res = apop();
        if (kind(res)!=booltype)
        { freeup(apop());
          freeup(res);
          buildfault("invalid test in RECUR");
          goto cleanup;
        }
        succ = boolval(res);
        freeup(res);
        if (succ)  /* test result is true, apply endf and append
                      to results, ehich is expanded if necessary */ 
        { do_apply(endf);
          if (j==jsize)
          { nialptr temp;
            jsize = jsize + incr;
            temp = new_create_array(atype,1,0,&jsize);
            copy(temp,0,results,0,j);
            freeup(results);
            results = temp;
          }
          store_array(results,j,apop());
          j++;
        }

        else  /* test result is false, apply structf to b */
        { nialint bsize;
          do_apply(structf);
          apush(top);
          bsize = tally(top);
          ishape(); /* get shape of result and leave on stack */
           
          /* make sure there is room in candidates for items of B, shape & marker */
          if (i+2+bsize>=isize)
          { nialptr temp;
            nialint diff = i+2+bsize-isize;
            nialint iincr = (incr>diff?incr:incr+diff);
            isize = isize + iincr;
            temp = new_create_array(atype,1,0,&isize);
            copy(temp,0,candidates,0,i);
            freeup(candidates);
            candidates = temp;
          }
          /* store the shape, marker and loop to store items of structf result */
          store_array(candidates,i++,apop());
          store_array(candidates,i++,marker);
          for (k=0;k<bsize;k++)
          { nialptr item;
            item = fetchasarray(top,bsize - 1 - k);
            store_array(candidates,i+k,item);
          }
          freeup(apop());
          i += bsize;
        }
      }
    }
    if (j==1)
      { apush(fetch_array(results,0));}
    else
      buildfault("system error in DOWN");
  cleanup:
    freeup(candidates);
    freeup(results);
    decrrefcnt(marker);
    freeup(marker);
  }
  if (closureflag) {
    decrrefcnt(closure);
    freeup(closure);
  }
}


/* routine to implement transformer TEAM as
     TEAM [f,g,..., z] [A,B,...,D] = [f A,g B,..., z D]
*/

void
iteam()
{
  nialptr     f,
              closure = Null;
  int         closureflag = false;

#ifdef PROFILE
  if (triggered || profile)  /* the argument has not been coerced */
#   else
  if (triggered)             /* the argument has not been coerced */
#   endif
  {
    if (tag(top) == t_closure) {  /* if a closure was done then it must be
                                     redone after the coercion so that an
                                     atlas won't be hidden. */
      nialptr     op = apop();

      apush(get_op(op));
      coerceop();
      closureflag = b_closure();
      if (closureflag) {
        incrrefcnt(top);     /* to protect closure */
        closure = top;
      }
      freeup(op);
    }
    else
      coerceop();
  }
  /* get the functional argument and validate it */
  f = apop();
  if (tag(f) == t_atlas) {
    if (tally(f) != 1 + tally(top)) {
      freeup(apop());
      buildfault("team arg mismatch");
    }
    else { 
      nialptr     x = apop();
      nialint     i,
                  tx = tally(x);
      int         v = valence(x);

      /* create result container */
      nialptr     z = new_create_array(atype, v, 0, shpptr(x, v));

      /* loop over data argument and atlas applying each function to the
         corresponding item */
      for (i = 0; i < tx; i++) {
        apush(fetchasarray(x, i));
        do_apply(fetch_array(f, i + 1));
        store_array(z, i, apop());
      }
      if (homotest(z)) /* test if the result is homogeneous */
        z = implode(z);
      apush(z);
      freeup(x);
    }
  }
  else /* if arg is not an atlas apply f directly to the argument */
    do_apply(f);
  if (closureflag) {
    decrrefcnt(closure);
    freeup(closure);
  }
}

/* routine to apply the transformer ITERATE f, which applies f to each item
   of the data argument in left to right order. The result is that of the
   last application. 

*/

void
iiterate()
{
  nialptr     x,
              f;
  nialint     i,
              tx;

  f = apop();
  x = apop();
  tx = tally(x);
  if (tx != 0) { /* arg not empty */

    /* loop over items apply f to each in turn. kee plast result */
    for (i = 0; i < tx; i++) {
      apush(fetchasarray(x, i));
      do_apply(f);
      if (i != tx - 1)
        freeup(apop()); /* all results except the last are ignored */
    }
  }
  else
    apush(Nullexpr);  /* result if data arg is empty */
  freeup(x);
}


/* routines to support the reduction and accumulation routines.
  The semantics are determined by:


#  1. Implement REDUCE r A = r A on a primitive reductive operation r
     as a right reduction. For all other operations f,
     REDUCE f on an empty will be a fault, but all non-empties
     will behave as expected.

REDUCE IS TR f OP A {
        % if f is reductive apply f directly else;
      IF empty A THEN
        Res := ??identity;
      ELSE
        Res := last A;
        FOR I WITH reverse front grid A DO
          Res := A@I f Res;
        ENDFOR;
      ENDIF;
      Res }


#  2. Implement LEFTREDUCE in a symmetric fashion.

LEFTREDUCE IS TR f OP A {
        % if f is reductive apply f directly else;
      IF empty A THEN
        Res := ??identity;
      ELSE
        Res := first A;
      FOR I WITH rest grid A DO
          Res := Res f A@I;
      ENDFOR;
      ENDIF;
      Res }


#  3. Implement ACCUMULATE as a right accumulation, but recognize that
for a reductive operation it can be done as a left one.

ACCUMULATE IS TR f OP A {
  heads IS OPERATION A {
   count tally A EACHLEFT take list A };
   % if f is reductive apply LEFTACCUMULATE else;
   shape A reshape EACH RREDUCE f heads A }

LEFTACCUMULATE IS TR f OP A {
      B := list A;
      Res := B;
      FOR I WITH rest grid B DO
        Res@I := Res@(I - 1) f B@I
      ENDFOR;
      shape A reshape Res }

*/


void
ireduce()
{
  nialptr     f;

  f = apop();
  if (tag(f) == t_basic && (get_prop(f) == 'R' || f == linkcode))
    do_apply(f);
  else if (f == pluscode)
    do_apply(sumcode);
  else if (f == timescode)
    do_apply(productcode);
  else
    reduce(f);
}

static void
reduce(nialptr f)
{
  nialptr     x,
              res;
  nialint     i,
              tx;

  x = apop();                /* get the argument */
  tx = tally(x);
  if (tx == 0) {             /* empty produces a fault */
    apush(makefault("?identity"));
  }                          /* freeup done below */
  else {
    res = fetchasarray(x, tx - 1);  /* get last item */

    /* loop from the right applying f to the pair formed
       from the item and the result so far */
    for (i = tx - 2; i >= 0; i--) {
      pair(fetchasarray(x, i), res);
      do_apply(f);              /* A@I f Res */
      res = apop();
    }
    apush(res);
  }
  freeup(x);
}


static void
leftaccumulate(nialptr f)
{
  nialptr     x,
              res,
              z;
  nialint     i,
              tx;
  int         vx;

  if (tally(top) <= 1)
    return;                  /* accumulate on an empty or solitary has no effect */
  x = apop();                /* get the argument */
  tx = tally(x);
  vx = valence(x);

  /* create the result container */
  z = new_create_array(atype, vx, 0, shpptr(x, vx));
  res = fetchasarray(x, 0);  /* get first item */
  store_array(z, 0, res);

  /* loop from the left applying f to the pair formed
       from the result so far and the item */
  for (i = 1; i < tx; i++) {
    pair(res, fetchasarray(x, i));
    do_apply(f);
    res = apop();
    store_array(z, i, res);
  }
  if (homotest(z)) /* test if the result is homogeneous */
    z = implode(z);
  apush(z);
  freeup(x);
}

/* routine to implement the primitive transformer ACCUMULATE */

void
iaccumulate()
{
  nialptr     f;

  f = apop();
  if (tag(f) == t_basic && (get_prop(f) == 'R' || f == linkcode || f == pluscode
                            || f == timescode))
    leftaccumulate(f);
  else
    accumulate(f);
}

static void
accumulate(nialptr f)
{
  nialptr     x,
              res,
              z;
  nialint     i,
              j,
              tx;
  int         vx;

  if (tally(top) <= 1)
    return;                  /* accumulate on an empty or solitary has no effect */
  x = apop();                /* get the argument */
  tx = tally(x);
  vx = valence(x);

  /* create the result container */
  z = new_create_array(atype, vx, 0, shpptr(x, vx));

  res = fetchasarray(x, 0);
  store_array(z, 0, res);    /* store first item */

  /* loop over the lengths being reduced */
  for (i = 1; i < tx; i++) {
    res = fetchasarray(x, i);/* get last item for making entry i */

    /* loop to do reduction of partial array */
    for (j = i - 1; j >= 0; j--) {
      pair(fetchasarray(x, j), res);
      do_apply(f);
      res = apop();
    }
    store_array(z, i, res);  /* store the result */
  }
  if (homotest(z)) /* test if the result is homogeneous */
    z = implode(z);
  apush(z);
  freeup(x);
}



/* routine to implement the tranformer N RANK f A, which applies 
   f to the subarrays of f of rank N and mixes the result.

   The formal definition is:

   RANK IS TRANSFORMER f OPERATION N A {  
        mix EACH f (N lower A) } 

*/

void
irank()
{
  nialptr     f = apop();

  /* test if the data arg is a special fault and return it */
  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;

  if (tally(top) != 2) {
    freeup(apop());          /* remove arg */
    apush(makefault("?argument to a RANK transform must be a pair"));
  }
  else {
    nialptr     x,
                nn,
                a;
    nialint     n;

    x = apop();
    if (tally(x) == 2) {
      splitfb(x, &nn, &a);
      if (kind(nn) == inttype && tally(nn) == 1) {
        n = intval(nn);
        if (n >= 0 && n <= valence(a))
          rank(f, n, a);
        else {
          apush(makefault("?left arg to RANK transform out of range"));
          freeup(a);
        }
      }
      else {
        apush(makefault("?invalid left arg to RANK transform"));
        freeup(a);
      }
      freeup(nn);
    }
    else
      apush(makefault("?arg to RANK transform must be a pair"));
    freeup(x);
  }
}

static void
rank(nialptr f, nialint n, nialptr a)
{
  int         va = valence(a),
              ka = kind(a),
              outv,          /* valence of result */
              kitem,
              vitem,         /* kind and valence of result slices */
              kres,          /* kind of result container */
              m = va - n;
  nialint     i;
  nialint    *argshape,      /* ptr to shape of a */
             *outshape = 0,  /* space for result shape */
              itemtally,     /* tally of an arg slice */
              restally,      /* tally of a result slice */
              outtally,      /* tally of result */
              nextargpos,    /* position in arg to start a slice */
              nextrespos;    /* position in result to place a slice */

  nialptr     it,
              resitem,
              res;


  if (atomic(a)) {           /* N must be 0 and result obeys  
                                  0 RANK f A = single f A */
    apush(a);
    do_apply(f);
    isingle();
    return;
  }

  if (tally(a) == 0) 
  { /* result is the argument */
    apush(a);
    return;
  }

  if (n == 0) /* the slices are the singles of the array items */
  { 
    /* implement directly as mix EACH f EACH single A to get details correct. */
    int_each(isingle, a);
    res = apop();
    each(f, res);
    imix();
    return;
  }

  /* compute the first item of the raised argument and apply f to get
     characteristics for the result container. */

  /* create container for first subarray and fill it */
  argshape = shpptr(a, va);
  it = new_create_array(ka, n, 0, argshape + m);  /* create container */
  itemtally = tally(it);
  nextargpos = 0;
  copy(it, 0, a, nextargpos, itemtally);  /* fill it with first slice from a */
  nextargpos += itemtally;

  /* push it, apply f, and determine result type */
  apush(it);
  do_apply(f);
  resitem = apop();          /* slice of result is in resitem */
  kitem = kind(resitem);
  kres = (kitem >= phrasetype ? atype : kitem); /* allow for phrase and faults */
  restally = tally(resitem);
  vitem = valence(resitem);
  outv = m + vitem;
  if (outv > 0) {            /* compute the result shape if not empty */
    outshape = (nialint *) malloc(outv * sizeof(nialint));
    if (outshape == NULL) {
      freeup(resitem);
      freeup(a);
      exit_cover1("unable to allocate space in RANK",NC_WARNING);
    }

    argshape = shpptr(a, va);
    for (i = 0; i < m; i++)
      outshape[i] = argshape[i];
    for (i = m; i < outv; i++)
      outshape[i] = pickshape(resitem, i - m);
  }

  /* create the result container */
  res = new_create_array(kres, outv, 0, outshape);
  outtally = tally(res);
  if (outtally != 0) {  /* result is not empty */
   /* copy the first slice into the result */
    nextrespos = 0;
    if (kres == kind(resitem))
      copy(res, nextrespos, resitem, 0, restally);
    else
      store_array(res, 0, resitem);
    nextrespos += restally;
    freeup(resitem);

    /* major loop to do the rest of the slices */
    while (nextrespos < outtally) {
      argshape = shpptr(a, va);
      /* create the container for the subarray  and fill it */
      it = new_create_array(ka, n, 0, argshape + m);
      copy(it, 0, a, nextargpos, itemtally);
      nextargpos += itemtally;

      /* push it, apply f, and store f result in final result */
      apush(it);
      do_apply(f);
      resitem = apop();
      if (kind(resitem) != kitem)
        goto mismatch;
      if (valence(resitem) != vitem)
        goto mismatch;
      for (i = 0; i < vitem; i++)
        if (outshape[m + i] != pickshape(resitem, i))
          goto mismatch;
      /* next item is consistent with first item */
      if (kres == kind(resitem))
        copy(res, nextrespos, resitem, 0, restally);
      else
        store_array(res, nextrespos, resitem);
      nextrespos += restally;
      freeup(resitem);
    }
  }
  else
    freeup(resitem);         /* the resitem was not freed above */
  apush(res);
  freeup(a);
  free(outshape);
  return;
mismatch:
  apush(makefault("?inconsistent results in RANK transform"));
  freeup(resitem);
  freeup(res);
  freeup(a);
  free(outshape);
}



/* routine to implement the transformer A BYKEY f B , which applies f to the collections
   of items of B formed by equal corresponding values in A.

The formal definition is:

BYKEY is TR f OP A B {
 A := list A ;
       Indices gets gradeup A;
       EACH f(cull A EACHLEFT findall sortup A
           EACHLEFT choose (Indices choose B)) }
*/

void
ibykey()
{
  nialptr     f,
              x,
              a,
              b,
              keys,
              z,
              sa,
              ind;
  nialint     tkeys,
              i;

  /* get the functional and data arguments and validate the latter */
  f = apop();
  x = apop();
  if (tally(x) != 2) {
    apush(makefault("?BYKEY transform expects a pair"));
    freeup(x);
    return;
  }

  /* split the data arg into a and b and protect a */
  splitfb(x, &a, &b);
  apush(a);                  /* to protect a for below */

  /* get the gradeup of a */
  sort(upcode, a, true);
  ind = top;                 /* indices to sort a, left on stack */
  choose(b, ind);
  b = top;                   /* b reordered to sa's order, left on stack to
                                protect it */
  swap();                    /* to put ind on top */
  choose(a, apop());         /* reuse the indices to sort a */
  sa = top;                  /* sorted a, left on top to protect it */
  set_sorted(sa, true);      /* we know this is sorted by up,
                                mark it for fast cull */
  cull(a, false);
  keys = apop();             /* keys in order they occured in a */
  tkeys = tally(keys);

  /* create the result container */
  z = new_create_array(atype, 1, 0, &tkeys);

  /* loop over the keys finding their places in sa, select corresponding items
     of b and apply f to them as an array */
     
  for (i = 0; i < tkeys; i++) {
    nialptr     keyitem = fetchasarray(keys, i);

    findall(keyitem, sa);
    choose(b, apop());
    if (tag(f) == t_basic) { /* apply f as a primitive */
      APPLYPRIMITIVE(f);

#ifdef FP_EXCEPTION_FLAG
      fp_checksignal();
#endif
    }
    else
      do_apply(f);  /* apply f as a parse tree */
    store_array(z, i, apop());  /* store the item of the result */
  }

  if (homotest(z)) /* test if the result is homogeneous */
    z = implode(z);
  freeup(apop());            /* unprotect and freeup sa */
  freeup(apop());            /* unprotect and freeup b */
  freeup(apop());            /* unprotect and freeup a */
  freeup(keys);
  freeup(x);
  apush(z);
}

