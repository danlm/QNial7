/*==============================================================

  MODULE   BLDERS.C

 COPYRIGHT NIAL Systems Limited  1983-2016

  This contains parse tree constructors and selectors.
  Others are macros in blders.h based on these and general selectors.

================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

/* standard library header files */

/* MATHLIB */
#include <math.h>

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

/* STDLIB */
#include <stdlib.h>

/* LIMITLIB */
#include <limits.h>

/* SJLIB */
#include <setjmp.h>



/* Q'Nial header files */

#include "blders.h"
#include "getters.h"         /* getters could be in blders.h */
#include "qniallim.h"
#include "absmach.h"
#include "lib_main.h"

#include "parse.h"           /* the parse tree tags */
#include "roles.h"           /* roles for identifiers */
#include "states.h"          /* scanner states, needed to build constants */
#include "symtab.h"          /* for sym_name  */



/* general builders. Have to have separate routines in the case
   where the node is all integers to avoid exploded homogeneous lists */

/* make list of integers of length one*/

nialptr
solitaryint(int n)
{
  nialptr     z;
  nialint     one = 1;

  z = new_create_array(inttype, 1, 0, &one);
  store_int(z, 0, n);
  return (z);
}

/* make pair of atype */

nialptr
mkapair(nialptr x, nialptr y)
{
  nialptr     z;
  nialint     two = 2;

  z = new_create_array(atype, 1, 0, &two);
  store_array(z, 0, x);
  store_array(z, 1, y);
  return (z);
}

/* make triple of atype */

nialptr
mkatriple(nialptr x, nialptr y, nialptr w)
{
  nialptr     z;
  nialint     three = 3;

  z = new_create_array(atype, 1, 0, &three);
  store_array(z, 0, x);
  store_array(z, 1, y);
  store_array(z, 2, w);
  return (z);
}

/* make quadruple of atype */

nialptr
mkaquad(nialptr x, nialptr y, nialptr w, nialptr u)
{
  nialptr     z;
  nialint     four = 4;

  z = new_create_array(atype, 1, 0, &four);
  store_array(z, 0, x);
  store_array(z, 1, y);
  store_array(z, 2, w);
  store_array(z, 3, u);
  return (z);
}

/* make triple of ints */

nialptr
mkitriple(nialint i0, nialint i1, nialint i2)
{
  nialptr     z;
  nialint     three = 3;

  z = new_create_array(inttype, 1, 0, &three);
  store_int(z, 0, i0);
  store_int(z, 1, i1);
  store_int(z, 2, i2);
  return (z);
}

/* make quint of ints */

nialptr
mkiquint(nialint i0, nialint i1, nialint i2, nialint i3, nialint i4)
{
  nialptr     z;
  nialint     five = 5;

  z = new_create_array(inttype, 1, 0, &five);
  store_int(z, 0, i0);
  store_int(z, 1, i1);
  store_int(z, 2, i2);
  store_int(z, 3, i3);
  store_int(z, 4, i4);
  return (z);
}

/* specific builders */

nialptr
b_opform(nialptr sym, nialptr cenv, int nvars, nialptr arglist, nialptr body)
{
  apush(createint((nialint) t_opform));
  apush(sym);
  apush(cenv);
  apush(createint((nialint) nvars));
  apush(arglist);
  apush(body);
  mklist(6);
  return (apop());
}

nialptr
b_blockbody(nialptr locallist, nialptr nonlocallist, nialptr defs, nialptr body)
{
  apush(createint((nialint) t_blockbody));
  apush(locallist);
  apush(nonlocallist);
  apush(defs);
  apush(body);
  mklist(5);
  return (apop());
}

nialptr
b_trform(nialptr sym, nialptr env, nialptr opargs, nialptr trbody)
{
  apush(createint((nialint) t_trform));
  apush(sym);
  apush(env);
  apush(opargs);
  apush(trbody);
  mklist(5);
  return (apop());
}

nialptr
b_ifexpr(nialint cnt)
{
  mklist(cnt);
  return (apop());
}

 /* builder for casr expression. both vals and exprs needed for selectors
  * tags. The vals are used in eval and the exprs in deparse */

nialptr
b_caseexpr(nialptr ctest, nialptr selectvals, nialptr selectexprs, nialptr exprseqs)
{
  apush(createint(t_caseexpr));
  apush(ctest);
  apush(selectvals);
  apush(selectexprs);
  apush(exprseqs);
  mklist(5);
  return (apop());
}


nialptr
b_block(nialptr sym, nialptr cenv, int nvars, nialptr body)
{
  apush(createint(t_block));
  apush(sym);
  apush(cenv);
  apush(createint(nvars));
  apush(body);
  mklist(5);
  return (apop());
}


nialptr
b_curried(nialptr op, nialptr argexpr)
{
  return (mkatriple(createint(t_curried), op, argexpr));
}

nialptr
b_transform(nialptr tr, nialptr argop)
{
  return (mkatriple(createint(t_transform), tr, argop));
}

/* getters */

nialptr
get_sym(nialptr x)
{
  if (kind(x) == atype)
    return ((nialptr) intval(fetch_array(x, 1)));
  else
    return ((nialptr) fetch_int(x, 1));
}

int
tag(nialptr x)
{
  if (kind(x) == atype)
    return (intval(fetch_array(x, 0)));
  else
    return (fetch_int(x, 0));
}

nialptr
get_name(nialptr x)
{
  if (tag(x) == t_identifier)
    return (get_id(x));
  else
    return (get_var_pv(x));
}

nialptr
get_entry(nialptr x)
{
  if (kind(x) == atype)
    return ((nialptr) intval(fetch_array(x, 2)));
  else
    return ((nialptr) fetch_int(x, 2));
}

/* b_opcall does an optimization to recognize binary basic operations
   in infix calls. This includes recognizing infix uses of sum (+) etc.
   */

nialptr
b_opcall(nialptr op, nialptr argexpr)
{
  if (tag(op) == t_curried) {
    nialptr     op1 = get_op(op);
    int         prop = get_prop(op1);

    if (tag(op1) == t_basic && (prop == 'B' || prop == 'C' || prop == 'R')) {
      nialptr     tree =
      mkaquad(createint(t_basic_binopcall), op1, get_argexpr(op), argexpr);

      freeup(op);
      return tree;
    }
  }
  return (mkatriple(createint(t_opcall), op, argexpr));
}

/* the constant builder must take the tkn and a state
   and convert the token into the appropriate constant
   */

nialptr
b_constant(int state, nialptr tkn)
{
  nialptr     v = Null;      /* initialized to avoid warning in gcc */
  char       *buffer;
  nialint     length,
              flength,
              m,
              k,
              n;
  int         c;
  double      r;

  buffer = pfirstchar(tkn);  /* reinitialized below if allocation done */
  flength = strlen(buffer);
  switch (state) {
    case sFChar:
        v = createchar(buffer[1]);
        break;
    case sString1:           /* count internal double quotes */
        n = 0;
        length = flength - 1;
        for (k = 1; k < length; k++) {
          if (buffer[k] == '\'') {
            n++;             /* to count */
            k++;             /* to skip second quote */
          }
        }
        n = flength - 2 - n;
        if (n == 0)
          v = Null;
        else {
          v = new_create_array(chartype, 1, 0, &n);
          buffer = pfirstchar(tkn); /* in case heap has moved */
          m = 0;
          for (k = 1; k < length; k++) {
            c = buffer[k];
            store_char(v, m, c);
            m++;
            if (c == '\'')
              k++;           /* to skip second quote */
          }
        }
        break;


    case sInteger:
          
          
#ifdef INTS32
          /* get constant as double, check size and then convert to integer */
        r = atof(&buffer[0]);
        if ((r <= LARGEINT) && (r >= SMALLINT))
          
            v = createint((nialint) atol(&buffer[0]));
        else
          v = createreal(r); /* accept the constant as a real */
        break;
#elif INTS64
        v = createint((nialint) atoll(&buffer[0]));
        break;
#else
#error code needed in b_contant for this precision choice
#endif
    case sReal1:
    case sReal4:
        r = atof(&buffer[0]);
        v = createreal(r);
        break;

    case sPhrase:
        v = makephrase(&buffer[1]);
        break;

    case sFault:
        /* avoid triggering on a fault constant */
        v = createatom(faulttype, &buffer[1]);
        break;

    case sBool:
        if (flength == 1)
          v = (buffer[0] == 'l' || buffer[0] == 'L' ? True_val : False_val);
        else {
          v = new_create_array(booltype, 1, 0, &flength);
          buffer = pfirstchar(tkn); /* in case heap has moved */
          for (k = 0; k < flength; k++) {
            n = buffer[k];
            store_bool(v, k, (n == 'l' || n == 'L' ? 1 : 0));
          }
        }
        break;
  }
  return (mkatriple(createint(t_constant), v, tkn));
}

/* builder that combines a defn sequence with an expression
   or an expression sequence and pretends it is a defn_seq.
   This is a hack. It depends on eval, actually evaluating defns
   which is not really necessary. Should replace this with a
   special blder that keeps the parts separate. */

nialptr
combinedaction(nialptr defs, nialptr exprs)
{
  nialptr     res;
  nialint     i,
              len;

  if (tag(exprs) == t_exprseq) {  /* add items of exprseq to defn seq */
    len = tally(defs) + tally(exprs) - 1;
    res = new_create_array(atype, 1, 0, &len);
    for (i = 0; i < tally(defs); i++)
      store_array(res, i, fetch_array(defs, i));
    for (i = tally(defs); i < len; i++)
      store_array(res, i, fetch_array(exprs, i - tally(defs) + 1));
  }
  else { /* add single expr to defn seq */
    len = tally(defs) + 1;
    res = new_create_array(atype, 1, 0, &len);
    for (i = 0; i < tally(defs); i++)
      store_array(res, i, fetch_array(defs, i));
    store_array(res, len - 1, exprs);
  }
  freeup(defs);
  freeup(exprs);
  return res;
}


