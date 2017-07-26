 /*==============================================================

  MODULE   INSEL.C

  COPYRIGHT NIAL Systems Limited  1983-2016


  The routines to implement operations for selection and
  insertion of items in arrays. The index related cases from eval.c 
  are handled by routines here.

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

/* STLIB */
#include <string.h>

/* SJLIB */
#include <setjmp.h>

/* Q'Nial header files */

#include "insel.h"
#include "qniallim.h"
#include "absmach.h"
#include "basics.h"
#include "lib_main.h"

#include "ops.h"             /* for splitfb */
#include "roles.h"           /* for role codes */
#include "eval.h"            /* for eval, storevar etc. */
#include "utils.h"           /* for getuppername */
#include "trs.h"             /* for int_eachboth */
#include "fileio.h"          /* for nrintf and codes */
#include "blders.h"          /* for get routines */
#include "getters.h"         /* for get macros */
#include "parse.h"           /* for parse tree node tags */
#include "symtab.h"          /* for symbol table macros */



static int  pick(nialptr a, nialptr addr);
static int  reach(nialptr a, nialptr path);
static int  place(nialptr a, nialptr addr, nialptr x, int *changed);
static int  deepplace(nialptr a, nialptr path, nialptr x, int *changed);
static int  pathup(nialptr target, nialptr path, nialint i, nialint cnt, nialptr val, int *changed);
static int  placeall(nialptr a, nialptr addrs, nialptr vals, int *changed);
static void splace(nialptr entr, nialptr sym, nialptr addr, nialptr x);
static void sdeepplace(nialptr entr, nialptr sym, nialptr path, nialptr val);
static void splaceall(nialptr entr, nialptr sym, nialptr addrs, nialptr vals);
static nialptr copy_array(nialptr a);
static int  slice_addrs(nialptr a, nialptr addr);


/* The routines below implement the Nial selection operations pick, choose
   and reach. A pick B selectes the item from array B at address A. Choose is used 
   to select multiple items frlom an array and reach is used to select a subarray
   at an arbitrary depth in hte array.
   
   Pick is the primitive operation. The other two are defined
   from it as follows:

   choose IS OP A B { A EACHLEFT pick B }
	 
   reach IS P A {  
        IF empty P THEN  
           A  
        ELSE  
           rest P reach (first P pick A)  
        ENDIF } 

   If (list A) is not a valid address for B, then the fault ?address
   is returned since an empty array has no valid addresses.

   The internal routine pick(a,addr) implements the core semantics of operation 
   pick. It is described below. 

   The routines choose(a,addrs) and reach(a,path) are internal routines that
   provide fast implementations of the semantics implied by the definitions above.

  */

void
b_pick(void)
{
  nialptr     y = apop(),
              x = apop();

  if (!pick(y, x))
  {
    apush(makefault("?address"));
  }
}

void
ipick(void)
{
  nialptr     xx,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  xx = apop();
  if (tally(xx) != 2) {
    apush(makefault("?argument of pick must be a pair"));
  }
  else {
    splitfb(xx, &x, &y);
    if (!pick(y, x))
    {
      apush(makefault("?address"));
    }
  }
  freeup(xx);
}


/* implements the Nial primitive operation reach */

void
b_reach(void)
{
  nialptr     y = apop(),
              x = apop();

  if (!reach(y, x))
    apush(makefault("?path"));
}

void
ireach(void)
{
  nialptr     xx,
              x,
              y;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  xx = apop();
  if (tally(xx) != 2) {
    apush(makefault("?argument of reach must be a pair"));
  }
  else {
    splitfb(xx, &x, &y);
    if (!reach(y, x))
      apush(makefault("?path"));
  }
  freeup(xx);
}


/* implements the Nial primitive operation choose */

void
b_choose()
{
  nialptr     y = apop(),
              x = apop();

  if (!choose(y, x)) {       /* redo as EACHLEFT pick */
    if (!atomic(y)) {
      apush(y);              /* achieve eachleft by singling the right
                              * argument and using the internal version of
                              * eachboth */
      isingle();
      y = apop();
    }
    apush(x);                /* to protect it */
    apush(y);                /* to protect it */
    int_eachboth(b_pick, x, y);
    swap();
    freeup(apop());          /* free y */
    swap();
    freeup(apop());          /* free x */
  }
}

void
ichoose()
{
  nialptr     xx,
              x,
              y,
              z;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  xx = apop();
  if (tally(xx) != 2) {
    apush(makefault("?argument of choose must be a pair"));
    freeup(xx);
    return;
  }
  splitfb(xx, &x, &y);
  if (!choose(y, x)) {       /* redo as EACHLEFT pick */
    if (!atomic(y)) {
      apush(y);              /* achieve eachleft by singling the right
                              * argument and using the internal version of
                              * eachboth */
      isingle();
      y = apop();
    }
    apush(x);                /* to protect it */
    apush(y);                /* to protect it */
    int_eachboth(b_pick, x, y);
    swap();
    freeup(apop());          /* free y */
    swap();
    freeup(apop());          /* free x */
  }
  freeup(xx);
}

/* The internal routine pick is used to support the many routines that use
   item selection. It picks the item of an array a at address addr.
   It returns false if the address is out of range.
   The routine frees a and addr if they are temporary.
   It treats a 1-list integer as an integer for selecting in a list.
   */

static int
pick(nialptr a, nialptr addr)
{
  nialint     index = -1,
              tlyaddr,
              ithaddr,
              ithshape,
              i;
  int         validaddr,
              va,
              kaddr;

  validaddr = true;
  va = valence(a);
  tlyaddr = tally(addr);
  kaddr = kind(addr);
  /* check that the address has reasonable structure, coercing a solitary
   * integer to an integer. */
  if (va == tlyaddr) {       /* valence matches address length */
    if (tlyaddr == 0) {      /* picking from a single */
      apush(a);
      ifirst();
      freeup(addr);
      return (true);
    }
    else if (kaddr == inttype) {  /* address has integer items */
      if (tlyaddr == 1) {    /* picking in a list */
        index = fetch_int(addr, 0); /* works for any valence */
        if (index < 0 || index >= tally(a))
          validaddr = false;
      }
      else {         /* The target has 2 or more axes. Convert the
                        address to an index and test in range in one loop.  */
        nialint    *shp = shpptr(a, va);  /* beginning of shape. Safe to
                                             store since no heap creations
                                             being done. */

        index = 0;
        for (i = 0; i < tlyaddr; i++) { 
          /* do Horner Rule evaluation of index */
          ithaddr = fetch_int(addr, i);
          ithshape = *(shp + i);
          if (ithaddr < 0 || ithaddr >= ithshape)
            validaddr = false;
          index = index * ithshape + ithaddr;
        }
      }
    }
    else                     /* non-integer index items */
      validaddr = false;
  }

  else                       /* valence mismatch */
    validaddr = false;
  if (validaddr) {           /* the address is in range for the array.
                                push the item on the stack */
    if (kind(a) == atype) {
      apush(fetch_array(a, index));
    }
    else
      apush(fetchasarray(a, index));
    freeup(a);
    freeup(addr);
  }
  else {
    freeup(addr);
    freeup(a);
  }

  return (validaddr);
}


/* The routine ifirst() in atops.c logically belongs here. However, it is
   left where it is for historical reasons. */

/* routine to implement the definition
       second IS OP A { 1 pick A } 
   directly */

void
isecond()
{
  nialptr     x,
              z;
  nialint     tx;

  x = apop();
  tx = tally(x);
  if (tx >= 2)
    z = fetchasarray(x, 1);
  else
    z = makefault("?address");
  apush(z);
  freeup(x);
}


/* routine to implement the definition
       third IS OP A { 2 pick A } 
   directly */


void
ithird(void)
{
  nialptr     x,
              z;
  nialint     tx;

  x = apop();
  tx = tally(x);
  if (tx >= 3)
    z = fetchasarray(x, 2);
  else
    z = makefault("?address");
  apush(z);
  freeup(x);
}


/* routine to reach the "piece" of array a at path-address path.
   It is used by ireach and the indexed select routine.
   It returns false if an address within the path is invalid.
   It frees a and path if temporary.
   This routine uses pick to do the selection along the path.
   */

static int
reach(nialptr a, nialptr path)
{
  nialptr     b,
              addr;
  nialint     i;
  int         cnt,
              validpath;

  apush(a);                  /* since pick might free it. */
  cnt = tally(path);
  validpath = true;
  apush(path);               /* to save it in case addr is the same */

  /* loop selecting item from array on the top of the stack */
  i = 0;
  apush(a);                  /* initialize loop by putting a on the stack */
  while (validpath && i < cnt) {
    b = apop();
    addr = fetchasarray(path, i);
    validpath = pick(b, addr);  /* pick frees b or addr if temporary */
    i++;
  }
  if (validpath)             /* swap computed value if any */
    swap();
  freeup(apop());            /* protected copy of path */
  if (validpath)             /* swap computed value if any */
    swap();
  freeup(apop());            /* protected copy of a */
  return (validpath);
}


/* The internal routine to choose the items of a at addresses addrs.
   It is considerably faster than using the above definition.
   The result is an array of the same shape as addrs.
   It is used by ichoose and the indexed select routines.
   It returns false if one of the addrs is not valid.
   It frees a and addrs if they are temporary.
   It uses pick to do the coercion and selection.
*/

int
choose(nialptr a, nialptr addrs)
{
  nialptr     z,
              addri;
  nialint     ta,
              addr,
              i;
  int         k,
              validaddrs,
              cnt,
              v,
              listcase;

    

  apush(a);    /* protect a since pick might free it before we are
                  done using it. */

  v = valence(addrs);
  ta = tally(a);
  
  /* There is special case code when a is a list and addrs is a list of 
     integers as noted by the flag set below. The case of addrs being 
     atomic is not handled  by the special listcase code. */

  listcase = valence(a) == 1 && v > 0 && kind(addrs) == inttype;
  cnt = tally(addrs);

  /* compute the kind of the result. 
     Note that if addrs is a single then atype is used  */

  k = (v == 0 || kind(a) == phrasetype || kind(a) == faulttype ? atype : kind(a));

  /* create the container */
  z = new_create_array(k, v, 0, shpptr(addrs, v));

  /* loop to fill up the array */
  validaddrs = true;
  i = 0;
  while (validaddrs && i < cnt) {
    if (listcase) {          /* do the work of pick directly */
      addr = fetch_int(addrs, i);
      validaddrs = addr >= 0 && addr < ta;
      if (validaddrs)
        copy1(z, i, a, addr);/* copy the item to the result */
    }
    else {
      addri = fetchasarray(addrs, i);
      if (atomic(addrs))
        apush(addrs);        /* protect in case addr and addrs are the same */
      validaddrs = pick(a, addri);  /* pick frees addri if temporary */

      if (validaddrs) {
        nialptr     obj = apop();

        if (homotype(k)) {
          copy1(z, i, obj, 0);  /* can copy because obj is of same kind */
          freeup(obj);
        }
        else
          store_array(z, i, obj);
      }
      if (atomic(addrs))
        apop();    /* remove extra copy of addrs for this special case */
    }
    i++;
  }
  if (validaddrs) {
    if (v == 0) {            /* addrs is a single */
      nialptr     obj = fetch_array(z, 0);

      if (atomic(obj)) {   /* result is the atom not the container holding it */
        apush(obj);
        freeup(z);           /* free container */
      }
      else
        apush(z);
    }
    else
    {  /* test for need of an implode after selection */
       if (kind(z) == atype && homotest(z))
          z = implode(z);
      apush(z);
    }
  }
  else
    freeup(z);
  if (validaddrs)            /* swap with the answer since there is one */
    swap();
  apop();                    /* to pop the protected copy of a */
  if (validaddrs) {          /* don't free args if it blows up. */
    freeup(addrs);           /* this is safe because we protect addrs in the
                                case when pick could free it.  */
    freeup(a);
  }
  return (validaddrs);
}



/* The routine iplace implements the Nial operation place
   place (X Addr) A
   which inserts X at address Addr in array A.
   An attempt to place out of range results in fault ?address.
 
   iplace uses the internal routine place to do its work.
 */

void
iplace(void)
{
  nialptr     z,
              xaddr,
              a,
              addr,
              x;
  int         changed = false;  /* needed as a dummy argument in place */

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of place must be a pair"));
    freeup(z);
    return;
  }
  splitfb(z, &xaddr, &a);
  if (tally(xaddr) != 2) {
    apush(makefault("?first argument of place must be a pair"));
    freeup(xaddr);
    freeup(a);         /* these are needed only if z is homogeneous */
    freeup(z);
    return;
  }
  splitfb(xaddr, &x, &addr);
  if (!place(a, addr, x, &changed)) /* changed is not examined. The freeup of
                                       z below will free a if it has changed
                                       and z is temporary */
    apush(makefault("?address"));
  freeup(xaddr);             /* needed if xaddr is homogeneous */
  freeup(z);
}

/* The internal routine place handles the updating in place of items in an
   array. It assumes that if the reference count is 1 or 0 then it
   is safe to directly replace a compatible item. This is safe
   because the array argument to place and placeall belonging to
   a variable will have refcnt 2, one because it is a variable
   and another because of the pair.
   Place returns the updated array as its result.

   An invalid address returns false so that the
   detailed error message to be determined at a higher level.
   If a is atomic and addr is Null then a new single is
   created.

   The routine explodes a homotype if the type of X doesn't match
   that of A.
   It calls copy_array if A is shared.

   If the resulting array is not the same container that was orginally 
   passed, then the variable changed is set to pass this information back to 
   the calling routine.

   The routine assumes changed has been set to false on the
   calling side.  The parameter changed is used within placeall to
   keep track of temporaries, and in the routines splace, splaceall, 
   and sdeepplace to avoid an unnecessary call on store_var.

   The routine frees X and Addr if either is temporary.
   It also frees A if it is temporary and not used in the result.
   The routine is modelled after pick. They share the same code until 
   either a selection or insertion is to be done. 
   This common code could be made a routine, but little would be gained by it.
*/

static int
place(nialptr a, nialptr addr, nialptr x, int *changed)
{
  nialint     index = -1,
              tlyaddr,
              i,
              ithaddr,
              ithshape;
  int         validaddr,
              va,
              kaddr;

  validaddr = true;
  va = valence(a);
  tlyaddr = tally(addr);
  kaddr = kind(addr);
  /* check that the address has reasonable structure, coercing a solitary
   * integer to an integer. */
  if (va == tlyaddr)         /* valence matches address length */
  { if (tlyaddr == 0)        /* placing into a single */
    { apush(x);
      isingle();
      freeup(a);
      freeup(addr);
      *changed = true;       /* new a on stack not old one */
      return (true);
    }
    else 
    { if (kaddr == inttype) {  /* address has integer items */
        if (tlyaddr == 1) {  /* picking in a list */
          index = fetch_int(addr, 0); /* works for any valence */
          if (index < 0 || index >= tally(a))
            validaddr = false;
        }
        else {    /* The target has 2 or more axes. Convert the
                     address to an index and test in range in one loop */

          nialint    *shp = shpptr(a, va);  /* beginning of shape. Safe to
                                             store since no heap creations
                                             being done. */

          index = 0;
          for (i = 0; i < tlyaddr; i++) { 
            /* do Horner Rule evaluation of index */
            ithaddr = fetch_int(addr, i);
            ithshape = *(shp + i);
            if (ithaddr < 0 || ithaddr >= ithshape)
              validaddr = false;
            index = index * ithshape + ithaddr;
          }
        }
      }
      else
        validaddr = false;
    }
  }
  else                       /* valence mismatch */
    validaddr = false;
  /* do the insertion */
  if (validaddr) {     /* the address is in range for the array.
                          Do the insertion, singles already handled above */

    if (homotype(kind(a))) { 
      /* explode a homogeneous array if x is not an atom of the same type */
      if (kind(a) != kind(x) || atomic(a) || valence(x) > 0) {
        a = explode(a, valence(a), tally(a), 0, tally(a));
        *changed = true;
      }
    }
    if (!*changed && refcnt(a) > 1) { 
      /* force a copy to unshare an array with 2 or more refcnts */
      a = copy_array(a);
      *changed = true;
    }
    if (kind(a) == atype) {
      replace_array(a, index, x); /* update the array */
      if (homotest(a)) {
        apush(x);            /* protect x in case part of a */
        a = implode(a);
        *changed = true;
        apop();
      }
    }
    else
      copy1(a, index, x, 0);

    set_sorted(a, false);    /* check sorted since since an insertion may 
                                destroy or create an ordering */
    apush(a);                /* push the result since the insertion is done */
  }

  freeup(addr);
  freeup(x);
  return (validaddr);
}


/* ideepplace implements the Nial operation deepplace,
     deepplace (X Path) A 
   places the subarray X in A at the level determined by Path.
   It returns an array with the same structure except for X being inserted.
   The argument to iplace is split and then the first item is split.
   A deeplace out of range returns the fault ?path.

   The Nial level routine ideepplace uses the internal routine deepplace to 
   do its work.
*/

void
ideepplace(void)
{
  nialptr     z,
              x,
              xpath,
              a,
              path;
  int         changed = false;  /* used for a dummy parameter to deepplace */

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of placeall must be a pair"));
    freeup(z);
    return;
  }
  splitfb(z, &xpath, &a);
  if (tally(xpath) != 2) {
    apush(makefault("?first argument of placeall must be a pair"));
    freeup(xpath);
    freeup(a);               /* these are needed only if z is homogeneous */
    freeup(z);
    return;
  }
  splitfb(xpath, &x, &path);
  if (!deepplace(a, path, x, &changed)) /* changed is not examined. The
                                           freeup of z below will free a if
                                           it has changed and z is temporary */
    apush(makefault("?path"));
  freeup(xpath);             /* needed if xpath is homogeneous */
  freeup(z);
}

/* The routine deepplace handles deep updates to values. It pushes
   the updated value or returns an error as appropriate.
   It uses a recursive internal routine pathup to go down through the levels.
   If the path traverses singles the it can result in telescoping or
   extension of the resulting array.
   The routine pathup uses place to explode a homotype if the
   type  of x doesn't match and to call copy_array to ensure
   that the updated array is unshared at each step.
   It frees x and path at the end. It assumes a is freed
   by pathup if temporary and replaced.
   The routine indicates whether the container has changed 
   using the parameter changed.
*/

static int
deepplace(nialptr a, nialptr path, nialptr x, int *changed)
{
  nialint     cnt,
              i;
  int         validpath;

  apush(x);                  /* to cause a copy in case x is a */
  if (!atomic(a))
    if (refcnt(a) > 1) {
      a = copy_array(a);
      *changed = true;
    }
  cnt = tally(path);
  apush(path);               /* to protect it during pathup */
  i = 0;
  validpath = pathup(a, path, i, cnt, x, changed);
  if (validpath)             /* swap with result if there is one */
    swap();
  freeup(apop());            /* protected path */
  if (validpath)             /* swap with result if there is one */
    swap();
  freeup(apop());            /* protected x */
  return (validpath);
}

/* pathup is the recursive routine used by deepplace.
   It uses pick and place to ensure proper handling of
   atoms and singles to achieve telescoping.
   Note that this routine only copies the subparts of a nested
   array that are shared on the path to the item to be changed.
   This permits updating of a global nested array without
   any copying.

   The parameter changed reports whether the target has been changed
   by the place. The recursive calls have a new variable
   in that position because it is only the top level change that
   we need to report to sdeepplace.

   i is the path item to use, cnt is the length of the path.
   */

static int
pathup(nialptr target, nialptr path, nialint i, nialint cnt, nialptr val, int *changed)
{
  nialptr     addr,
              newtarget,
              newval;
  int         validpath,
              ch = false;

  validpath = true;
  if (i == cnt) {
    /* end of recursion since all path items have been used */
    apush(val);              /* the value to be inserted */
    freeup(target);          /* in case target is temporary and not used */
    if (cnt == 0)            /* if cnt is 0 then deepplace is doing a
                                replacement at the top level */
      *changed = true;
  }
  else {
    /* prepare to do the place */
    addr = fetchasarray(path, i);
    apush(addr);             /* to preserve it for place */
    apush(target);           /* to preserve it for place */
    /* do the pick in the target */
    validpath = pick(target, addr);
    if (validpath) {
      newtarget = apop();
      if (!atomic(newtarget) && (refcnt(newtarget) > 1)) {
        /* make copy of shared array */ 
        newtarget = copy_array(newtarget);
      }
      /* do the recursion to continue down the path */
      validpath = pathup(newtarget, path, i + 1, cnt, val, &ch);
      if (validpath) {
        /* insertion below this level completed */
        newval = apop();
        apop();
        apop();    /* to unprotect addr and target and avoid
                      unneeded copying  and to allow place to free
                      them if temporary */
        /* insert the value returned by pathup into the target.
           It leave the updated result on the stack */
        place(target, addr, newval, changed);
      }
      /* the cleanup of newtarget is done in the recursive call to pathup in
         case the path fails at this point */
    }
    if (!validpath) {        /* test needed since validpath can change in
                                block immediately above the test */
      freeup(apop());        /* cleanup target */
      freeup(apop());        /* cleanup addr */
    }
  }
  return (validpath);
}


/* iplaceall implements the Nial operation placeall
   placeall (Vals Addrs) A
   which inserts the items of Vals at addresses Addrs in array A.
   A place out of range results in thef ault ?addresses.
 
   iplaceall uses the internal routine placeall to do its work.
   */

void
iplaceall(void)
{
  nialptr     z,
              xaddrs,
              a,
              x,
              addrs;
  int         changed = false;  /* needed for dummy parameter to placeall */

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument of placeall must be a pair"));
    freeup(z);
    return;
  }
  splitfb(z, &xaddrs, &a);
  if (tally(xaddrs) != 2) {
    apush(makefault("?first argument of placeall must be a pair"));
    freeup(xaddrs);
    freeup(a);               /* these are needed only if z is homogeneous */
    freeup(z);
    return;
  }
  splitfb(xaddrs, &x, &addrs);
  if (tally(x) != tally(addrs)) { /* reshape x to tally of addrs if tally's
                                     do not match */
    nialint     ta = tally(addrs);

    reshape(createint(ta), x);
    x = apop();
  }
  if (!placeall(a, addrs, x, &changed)) 
         /* changed is not examined. The freeup of z below will 
            free a if it has changed and z is temporary */
    apush(makefault("?addresses"));
  freeup(xaddrs);            /* needed if xaddrs is homogeneous */
  freeup(z);
}

/* Routine to place items of Val at addresses Addrs in A.
   Addrs and Vals must have the same shape, except singles
   are replicated. Returns false if any addresses are out of
   range.
   Frees Addrs and Vals if temporary and A if temporary and not used
   for the result.
   If the result is in a container different from A, the variable changed is set.
   It is used by splaceall to avoid an unnecessary call to store_var.
   */

static int
placeall(nialptr a, nialptr addrs, nialptr vals, int *changed)
{
  nialptr     v1 = 0,
              addr,
              val;
  nialint     cnt,
              i;
  int         v,
              validaddrs,
              valflag;

  v = valence(vals);
  cnt = tally(addrs);
  valflag = false;
  if (tally(addrs) != tally(vals) && v != 0) {
    /* mismatched shapes */
    freeup(addrs);
    freeup(vals);
    return (false);
  }
  if (v == 0)
    /* pull out item of single to replicate */
  {
    valflag = atomic(vals);
    if (valflag) {
      apush(vals);
    }                        /* to protect vals */
    else {
      v1 = vals;             /* to save it for freeing below */
      vals = fetch_array(vals, 0);
    }
  }
  i = 0;
  validaddrs = true;
  if (atomic(addrs))
    apush(addrs);    /* since addr is the same below and place can free it */
  apush(a);          /* loop invariant has a on stack */
  while (validaddrs && i < cnt) {
    val = (v == 0 ? vals : fetchasarray(vals, i));
    a = apop();              /* target for next place */
    addr = fetchasarray(addrs, i);
    validaddrs = place(a, addr, val, changed);
    if (!validaddrs && *changed)
      freeup(a);       /* it may be temporary from an earlier call on place */
    i++;
  }
  if (atomic(addrs)) {
    if (validaddrs)          /* swap with result if there is one */
      swap();                /* clean addrs up if protected above */
    freeup(apop());
  }
  else {
    freeup(addrs);
  }                          /* the braces are needed since freeup is a define */
  if (v == 0)
  { if (valflag)
    { if (validaddrs)        /* swap with result if there is one */
        swap();
      freeup(apop());        /* to free up vals */
    }
    else {
      freeup(v1);
    }                        /* braces needed */
  }
  else {
    freeup(vals);
  }                          /* braces needed */
  return (validaddrs);
}


/* routine to implement the Nial operation update:
        update Nm I A
   where Nm is the name of an array, I is an address and A is the value
   being assigned. It mimics the semantics of the Indexed assignment
              Nm@I := A
   The routine uses splace to do the work. 
   It gives an address fault if the index is out of range.
*/

void
iupdate(void)
{
  nialptr     sym,
              var,
              addr,
              x,
              arg,
              entr;

  arg = apop();
  /* if array, index, and item are homogeneous then invalid */
  if (homotype(kind(arg)))
    buildfault("bad parameter list in update");
  else if (tally(arg) != 3)
    buildfault("bad parameter list in update");
  else
    /* get symbol table entry and value */
  { var =fetch_array(arg, 0);
    if (varname(var, &sym, &entr, false, false)) {
      if (sym_role(entr) == Rvar) {
        addr = fetch_array(arg, 1);
        x = fetch_array(arg, 2);
        splace(entr, sym, addr, x);
      }
      else
        buildfault("not a variable");
    }
    else
      buildfault("invalid name");
  }
  freeup(arg);
}

/* routine to implement the Nial operation deepupdate:
        deepupdate Nm P A
   where Nm is the name of an array, P is a path and A is the value
   being assigned. It mimics the semantics of the indexed assignment
              Nm@@P := A
   The routine uses sdeepplace to do the work. 
   It gives an address fault if any of the indices is out of range.
*/

void
ideepupdate(void)
{
  nialptr     arg,
              var,
              sym,
              path,
              x,
              entr;

  arg = apop();
  /* if array, index, and item are homogeneous then invalid */
  if (homotype(kind(arg)))
    buildfault("bad parameter list in update");
  else if (tally(arg) != 3)
    buildfault("bad parameter list in update");
  else
  { /* get symbol table entry and value */ 
    var = fetch_array(arg, 0);
    if (varname(var, &sym, &entr, false, false)) {
      if (sym_role(entr) == Rvar) {
        path = fetch_array(arg, 1);
        x = fetch_array(arg, 2);
        sdeepplace(entr, sym, path, x);
      }
      else
        buildfault("not a variable");
    }
    else
      buildfault("invalid name");
  }
  freeup(arg);
}

/* routine to implement the Nial operation updateall:
        updateall Nm I A
   where Nm is the name of an array, I is an array of indices and A is the 
   array of values being assigned. 
   It mimics the semantics of the indexed assignment
              Nm#I := A
   The routine uses splaceall to do the work. 
   It gives an address fault if any of the indices is out of range.
*/

void
iupdateall(void)
{
  nialptr     arg,
              var,
              sym,
              entr,
              addrs,
              x;

  arg = apop();
  /* if array, index, and item are homogeneous then invalid */
  if (homotype(kind(arg)))
    buildfault("bad parameter list in update");
  else if (tally(arg) != 3)
    buildfault("bad parameter list in update");
  else
  { var = fetch_array(arg, 0);
    /* get symbol table entry and value */ 
    if (varname(var, &sym, &entr, false, false)) {
      if (sym_role(entr) == Rvar) {
        addrs = fetch_array(arg, 1);
        x = fetch_array(arg, 2);
        splaceall(entr, sym, addrs, x);
      }
      else
        buildfault("not a variable");
    }
    else
      buildfault("invalid name");
  }
  freeup(arg);
}

 /* select1 does the evaluation work for selection from an array
    using any of the three indexing notations: @, @@, # and |.

    The code in this routine could be moved to eval_fun.c by making
    the routines called in select1 external to this module.
 */

void
select1(nialptr exp)
{
  nialptr     addr,
              a;
  int         okay;

  /* evaluate the address part of the expression */
  eval(get_up_expr(exp));
  addr = apop();
  
  /* evaluate the identifier part and store in a */
  eval(get_up_id(exp));
  a = apop();

  /* switch on the indexing type being handled */
  switch (tag(exp)) {
    case t_pickplace:
        /* do the pick */
        okay = pick(a, addr);
        if (!okay) {
          buildfault("address");
        }
        break;
    case t_reachput:
        /* do the reach */
        okay = reach(a, addr);
        if (!okay)
          buildfault("path");
        break;
    case t_slice:
        if (valence(a) == 2 && kind(addr) == atype) { 
          /* do a special speed up for tables */
          /* select the rows and columns */
          nialptr     rows = fetch_array(addr, 0),
                      cols = fetch_array(addr, 1);
          /* see if it is a column slice */
          if (rows == Nullexpr && isint(cols)) {  /* pull out a column */
            nialint     i,
                        j,
                        k,
                        r,
                        c,
                       *shp = shpptr(a, valence(a));  /* Safe. just used to
                                                       * get r and c below. */

            nialptr     z;

            /* validate the column index */
            r = shp[0];
            c = shp[1];
            i = intval(cols);
            if (i < 0 || i >= c) {
              buildfault("address");
              freeup(addr);
              freeup(a);
              return;
            }
            /* create the container and loop to fill */
            z = new_create_array(kind(a), 1, 0, &r);
            j = 0;
            for (k = 0; k < r; k++) {
              switch (kind(a)) {
                case booltype:
                    store_bool(z, k, fetch_bool(a, i + j));
                    break;
                case inttype:
                    store_int(z, k, fetch_int(a, i + j));
                    break;
                case realtype:
                    store_real(z, k, fetch_real(a, i + j));
                    break;
                case chartype:
                    store_char(z, k, fetch_char(a, i + j));
                    break;
                case atype:
                    store_array(z, k, fetch_array(a, i + j));
                    break;
              }
              j += c;
            }
            /* push the result and clean up */
            apush(z);
            freeup(addr);
            freeup(a);
            return;
          }
          else /* look for a row slice */
          if (cols == Nullexpr && isint(rows)) { /* pull out a row */
            nialint     i,
                        r,
                        c,
                       *shp = shpptr(a, valence(a));  /* Safe. just used to
                                                         get r and c below. */
            nialptr     z;
            /* validate the row index */
            r = shp[0];
            c = shp[1];
            i = intval(rows);
            if (i < 0 || i >= r) {
              buildfault("address");
              freeup(addr);
              freeup(a);
              return;
            }
            /* special case for char type */
            if (kind(a) == chartype) {
              z = new_create_array(chartype, 1, 0, &c);
              strncpy(pfirstchar(z), pfirstchar(a) + i * c, c);
            }
            else {
              /* create container and fill it */
              z = new_create_array(kind(a), 1, 0, &c);
              copy(z, 0, a, i*c, c);
            }
            /* push result and clean up */
            apush(z);
            freeup(addr);
            freeup(a);
            return;
          }
        }

        /* use the general code to do a slice with choose */
        okay = slice_addrs(a, addr);
        if (!okay) {
          buildfault("slice");
          break;
        }
        addr = apop();
        /* fall through to choose case */
    case t_choose:
        okay = choose(a, addr);
        if (!okay) {
          if (tag(exp) == t_slice)
            buildfault("slice");
          else
            buildfault("addresses");
          freeup(addr);
        }
        break;
#ifdef DEBUG
    default:
        nprintf(OF_DEBUG, "bad case in select\n");
        nabort(NC_ABORT);
#endif
  }
}



/*  compute addresses for a slice notation A|I usedon the left or right.
    frees up addr if temporary.
*/

static int
slice_addrs(nialptr a, nialptr addr)
{
  nialptr     sa,
              it;
  nialint     i;
  int         va;

  /* get the shape and valence of A */
  apush(a);
  apush(a);                  /* to protect a in ishape() */
  ishape();
  sa = apop();
  apop();                    /* to unprotect a */
  va = valence(a);

  /* validate address is of right length */
  if (tally(addr) != va) {
    freeup(addr);
    freeup(sa);
    return (false);
  }

  /* convert atomic address to a one-list */
  if (atomic(addr)) {
    apush(addr);
    ilist();
    addr = apop();
  }

  /* unshare addr so we can play with it */
  if (refcnt(addr) >= 1)
    addr = copy_array(addr);
  /* if slice indicator has a Nullexpr then addr will be atype */
  if (kind(addr) == atype) {
    /* replace the Nulls with the index set for that dimension */
    for (i = 0; i < va; i++) {
      it = fetch_array(addr, i);
      if (it == Nullexpr)
        it = generateints(fetch_int(sa, i));
      replace_array(addr, i, it);
    }
  }
  freeup(sa);                /* freeup shape */
  apush(addr);
  /* use icart to generate all the addresses */
  icart();                   /* frees up addr */
  return (true);
}

/* insert does the evaluation work for insertion into an array
    using any of the four indexing notations: @, @@, # and |.

    The code in this routine could be moved to eval_fun.c by making
    the routines called in it external to this module.
*/


void
insert(nialptr exp)
{
  nialptr     leftexpr,
              val,
              a,
              addr,
              sym,
              entr;
  int         okay;

  leftexpr = get_ia_left(exp);
  eval(get_ia_expr(exp));    /* eval the replacement value */
  val = apop();
  a = get_up_id(leftexpr);   /* eval the id to get binding */
  sym = get_sym(a);
  entr = get_entry(a);
  eval(get_up_expr(leftexpr));  /* eval the indexing expr */
  addr = apop();
  switch (tag(leftexpr)) {   /* which type (@,@@,#,|) */
    case t_pickplace:        /* it's @ */
        splace(entr, sym, addr, val);
        break;
    case t_reachput: /* it's @@ */
        sdeepplace(entr, sym, addr, val);
        break;
    case t_slice: /* it's | */
        a = fetch_var(sym, entr);
        /* test if it is a slice of a table */
        if (valence(a) == 2 && kind(addr) == atype) {
          /* get the row and column indices */
          nialptr     rows = fetch_array(addr, 0),
                      cols = fetch_array(addr, 1);
          /* test if it is a column slice */
          if (rows == Nullexpr && isint(cols) && kind(a) == kind(val)) {  
            /* store into a column */
            nialint     i,
                        j,
                        k,
                        r,
                        c,
                       *shp = shpptr(a, valence(a));  /* Safe. just used to
                                                         get r and c below. */

            r = shp[0];
            c = shp[1];
            i = intval(cols);
            if (i < 0 || i >= c) {
              buildfault("address");
              freeup(addr);
              freeup(val);
              return;
            }
            if (refcnt(a) > 1)  /* force a copy to unshare an array with 2 or
                                   more refcnts */
              a = copy_array(a);
            if (valence(val) == 0 && r > 1) { /* replicate val */
              reshape(createint(r), val);
              val = apop();
            }
            /* loop over the row indices */
            j = 0;
            for (k = 0; k < r; k++) {
              switch (kind(a)) {
                case booltype:
                    store_bool(a, i + j, fetch_bool(val, k));
                    break;
                case inttype:
                    store_int(a, i + j, fetch_int(val, k));
                    break;
                case realtype:
                    store_real(a, i + j, fetch_real(val, k));
                    break;
                case chartype:
                    store_char(a, i + j, fetch_char(val, k));
                    break;
                case atype:
                    store_array(a, i + j, fetch_array(val, k));
                    break;
              }
              j += c;
            }
            apush(a);
            store_var(sym, entr, a);
            freeup(addr);
            freeup(val);
            return;
          }
          else 
          /* test if it is a row slice */
          if (cols == Nullexpr && isint(rows) && kind(a) == kind(val)) { 
          /* store into a row */
            nialint     i,
                        r,
                        c,
                       *shp = shpptr(a, valence(a));  /* Safe. just used to
                                                       * get r and c below. */

            r = shp[0];
            c = shp[1];
            i = intval(rows);
            if (i < 0 || i >= r) {
              buildfault("address");
              freeup(addr);
              freeup(val);
              return;
            }
            if (refcnt(a) > 1)  /* force a copy to unshare an array with 2 or
                                   more refcnts */
              a = copy_array(a);
            if (valence(val) == 0 && c > 1) { /* replicate val */
              reshape(createint(c), val);
              val = apop();
            }
            /* copy the items */
            copy(a, i*c, val, 0, c);
            apush(a);
            store_var(sym, entr, a);
            freeup(addr);
            freeup(val);
            return;
          }
        }
        /* use the general code to do a slice insertion with splaceall */
        okay = slice_addrs(a, addr);
        if (!okay) {
          buildfault("slice");
          break;
        }
        addr = apop();
        /* fall through to placeall case */
    case t_choose:  /* it's # */
        splaceall(entr, sym, addr, val);
        break;
#ifdef DEBUG
    default:
        nprintf(OF_DEBUG, "bad case in insert\n");
        nabort(NC_ABORT);
#endif
  }
}


/* routine to update an item of a variable.
   It is done in place if possible.
   The value of the variable is replaced if
   the container has changed.
   addr and x are freed by place if temporary.
   */

static void
splace(nialptr entr, nialptr sym, nialptr addr, nialptr x)
{
  nialptr     a;
  int         validaddr,
              changed = false,
              freeflag;

  a = fetch_var(sym, entr);
  if (x == a) {              /* the case of making an array a member of
                                itself. Set refcnt >= 2, forcing a copy to be
                                made  */
    apush(a);                /* to protect a */
    freeflag = true;
  }
  else
    freeflag = false;
  /* place leaves value on stack if validaddr */
  validaddr = place(a, addr, x, &changed);
  if (!validaddr)
    buildfault("address");
  else if (changed)
    store_var(sym, entr, top);  /* do the store in the variable */
  if (freeflag) {
    if (validaddr)           /* swap with result if there is one */
      swap();
    freeup(apop());
  }
}

/* routine to update a piece of a variable.
   It is done in place if possible.
   The value of the variable is replaced if
   any container on the path has changed.
   path and val are freed by deepplace if temporary.
*/

static void
sdeepplace(nialptr entr, nialptr sym, nialptr path, nialptr val)
{
  nialptr     a;
  int         validpath,
              changed = false;

  a = fetch_var(sym, entr);
  /* deepplace leaves value on stack if validpath */
  validpath = deepplace(a, path, val, &changed);
  if (!validpath)
    buildfault("path");
  else if (changed)
    store_var(sym, entr, top);  /* do the store in the variable */

}

/* routine to update multiple items of a variable.
   The insertions are done in place if possible.
   The value of the variable is replaced if
   the container has changed.
   addrs, vals are freed by choose if temporary.
*/

static void
splaceall(nialptr entr, nialptr sym, nialptr addrs, nialptr vals)
{
  nialptr     a;
  int         validaddrs,
              changed = false;

  a = fetch_var(sym, entr);
  /* placeall leaves value on stack if validaddrs */
  validaddrs = placeall(a, addrs, vals, &changed);
  if (!validaddrs)
    buildfault("addresses");
  else if (changed)
    store_var(sym, entr, top);  /* do the store in the variable */
}




/* routine to copy shared nonatomic arrays for updating.
   The returned array is not exploded and has refcnt 0.
   Must never call this with a phrase or fault.
   This utility is place here since it is only used in this module.
*/

static      nialptr
copy_array(nialptr a)
{
  nialptr     b;
  nialint     t = tally(a);
  int         v = valence(a),
              k = kind(a);

  b = new_create_array(k, v, 0, shpptr(a, v));
  copy(b, 0, a, 0, t);
  return (b);
}
