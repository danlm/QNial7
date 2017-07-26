/* ==============================================================

   MODULE     TRIG.C

  COPYRIGHT NIAL Systems Limited  1983-2016

   This module implements the trigometric Nial primitives.

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

/* SJLIB */
#include <setjmp.h>

/* MATHLIB */
#include <math.h>


/* Q'Nial header files */

#include "trig.h"
#include "qniallim.h"
#include "absmach.h"
#include "basics.h"
#include "lib_main.h"

#include "trs.h"             /* for int_each */



/* routine to implement pervasive operation sin */

void
isin()
{
  nialptr     x = apop();
  double      r;

  if (atomic(x)) {

    /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);

joinreal: r = sin(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
          apush(makefault("?sin"));
          break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }

  else /* use EACH to apply sin to items */
  if (kind(x) == realtype)
    real_each(sin, x);
  else
    int_each(isin, x);  /* use isin to achieve pervasive recursion */
}

/* routine to implement cos */

void
icos()
{
  nialptr     x = apop();
  double      r;

  if (atomic(x)) {

  /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);

joinreal: r = cos(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
          apush(makefault("?cos"));
          break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }
  else /* use EACH to apply cos to items */

  if (kind(x) == realtype)
    real_each(cos, x);
  else
    int_each(icos, x); /* use icos to achieve pervasive recursion */

}

/* routine to implement sinh */

void
isinh()
{
  nialptr     x = apop();
  double      r;

  if (atomic(x)) {

    /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);
joinreal: r = sinh(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
          apush(makefault("?sinh"));
          break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }

  else /* use EACH to apply sinh to items */
  if (kind(x) == realtype)
    real_each(sinh, x);
  else
    int_each(isinh, x); /* use isinh to achieve pervasive recursion */
}

/* routine to implement cosh */

void
icosh()
{
  nialptr     x = apop();
  double      r;


  if (atomic(x)) {

    /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);
joinreal: r = cosh(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
          apush(makefault("?cosh"));
          break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }
  else /* use EACH to apply cosh to items */
  if (kind(x) == realtype)
    real_each(cosh, x);
  else
    int_each(icosh, x); /* use icosh to achieve pervasive recursion */

}

/* routine to implement arcsin */

void
iarcsin()
{
  nialptr     x = apop();
  double      r;

  if (atomic(x)) {

    /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);
joinreal:
          if (r < -1. || r > 1.)
            goto joinerror;
          r = asin(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
joinerror: apush(makefault("?arcsin"));
           break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }
  else /* use EACH to apply arcsin to items */
  if (kind(x) == realtype) {
    nialint     i,
                tx = tally(x);
    double     *xptr = pfirstreal(x); /* safe */

    /* loop to check that all args are valid for asin */
    for (i = 0; i < tx; i++) {
      double      r = *xptr++;

      if (r < -1. || r > 1.)
        goto use_each;  /* use recursive case to handle faults */
    }
    real_each(asin, x); /* use real case */
  }
  else
use_each:
    int_each(iarcsin, x); /* use iarcsin to achieve pervasive recursion */
}


/* routine to implement arccos */

void
iarccos()
{
  nialptr     x = apop();
  double      r;

  if (atomic(x)) {

    /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);
joinreal: if (r < -1. || r > 1.)
            goto joinerror;

          r = acos(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
joinerror: apush(makefault("?arccos"));
           break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }

  else /* use EACH to apply arcsin to items */
  if (kind(x) == realtype) {
    nialint     i,
                tx = tally(x);
    double     *xptr = pfirstreal(x); /* safe */

    /* loop to check that all args are valid for acos */
    for (i = 0; i < tx; i++) {
      double      r = *xptr++;

      if (r < -1. || r > 1.)
        goto use_each; /* use recursive case to handle faults */
    }
    real_each(acos, x); /* use real case */
  }
  else
use_each:
    int_each(iarccos, x); /* use iarccos to achieve pervasive recursion */
}


/* routine to implement arctan */

void
iarctan()
{
  nialptr     x = apop();
  double      r;

  if (atomic(x)) {

    /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);
joinreal: r = atan(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
          apush(makefault("?arctan"));
          break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }

  else /* use EACH to apply arctan to items */
  if (kind(x) == realtype)
    real_each(atan, x);
  else
    int_each(iarctan, x); /* use iarctan to achieve pervasive recursion */
}


/* routine to implement exp */

void
iexp()
{
  nialptr     x = apop();
  double      r;

  if (atomic(x)) {

    /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);
joinreal:r = exp(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
          apush(makefault("?exp"));
          break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }
  else /* use EACH to apply exp to items */
  if (kind(x) == realtype)
    real_each(exp, x);
  else
    int_each(iexp, x); /* use iexp to achieve pervasive recursion */
}

/* routine to implement ln */

void
iln()
{
  nialptr     x = apop();
  double      r;

  if (atomic(x)) {

    /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);
joinreal:
          if (r <= 0.)
            goto joinerror;
          r = log(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
joinerror: apush(makefault("?ln"));
           break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }
  else /* use EACH to apply ln to items */
  if (kind(x) == realtype) {
    nialint     i,
                tx = tally(x);
    double     *xptr = pfirstreal(x); /* safe */

    /* loop to check validity of items */
    for (i = 0; i < tx; i++) {
      double      r = *xptr++;

      if (r <= 0.)
        goto use_each; /* use recursive case to get faults */
    }
    real_each(log, x); /* use real case */
  }
  else
use_each:
    int_each(iln, x); /* use iln to achieve pervasive recursion */
}

/* routine to implement log */

void
ilog()
{
  nialptr     x = apop();
  double      r;

  if (atomic(x)) {

    /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);
      joinreal:
          if (r <= 0.)
            goto joinerror;

          r = log10(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
joinerror: apush(makefault("?log"));
           break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }

  else /* use EACH to apply log to items */
  if (kind(x) == realtype) {
    nialint     i,
                tx = tally(x);
    double     *xptr = pfirstreal(x); /* safe */

    /* loop to check validity of items */
    for (i = 0; i < tx; i++) {
      double      r = *xptr++;

      if (r <= 0.)
        goto use_each; /* use recursive case to get faults */

    }
    real_each(log10, x); /* use real case */
  }
  else
use_each:
    int_each(ilog, x); /* use ilog to achieve pervasive recursion */
}

/* routine to implement sqrt */

void
isqrt()
{
  nialptr     x = apop();
  double      r;

  if (atomic(x)) {

    /* fetch argument and convert to real type */
    switch (kind(x)) {
      case booltype:
          r = fetch_bool(x, 0) * 1.0;
          goto joinreal;

      case inttype:
          r = fetch_int(x, 0) * 1.0;  /* join real case */
          goto joinreal;

      case realtype:
          r = fetch_real(x, 0);
      joinreal:
          if (r < 0.)
            goto joinerror;

          r = sqrt(r);
          apush(createreal(r));
          break;

      case chartype:
      case phrasetype:
joinerror: apush(makefault("?sqrt"));
           break;

      case faulttype:
          apush(x);
          break;
    }
    freeup(x);
  }

  else /* use EACH to apply log to items */
  if (kind(x) == realtype) {
    nialint     i,
                tx = tally(x);
    double     *xptr = pfirstreal(x); /* safe */

    /* loop to check validity of items */
    for (i = 0; i < tx; i++) {
      double      r = *xptr++;

      if (r <= 0.)
        goto use_each; /* use recursive case to get faults */

    }
    real_each(sqrt, x); /* use real case */
  }
  else
use_each:
    int_each(isqrt, x); /* use isqrt to achieve pervasive recursion */
}

/* routine to implement tan using formula tan(x) = sin(x)/cos(x) */

void
itan()
{
  apush(top);                /* need the arg twice, protect it */
  isin();                    /* compute sin */
  swap();                    /* move arg on top */
  icos();                    /* compute cos */
  b_divide();                
}

/* routine to implement tanh using formula tanh(x) = sinh(x)/cosh(x) */

void
itanh()
{
  apush(top);                 /* need the arg twice, protect it */
  isinh();                    /* compute sinh */
  swap();                     /* move arg on top */
  icosh();                    /* compute cosh */
  b_divide();
}

