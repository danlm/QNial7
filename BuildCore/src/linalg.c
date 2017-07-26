/* ==============================================================

   MODULE   LINALG.C

  COPYRIGHT NIAL Systems Limited  1983-2016



   linear algebra operations implemented as primitives.

================================================================*/

/* Q'Nial file that selects features  */

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


/* STDLIB */
#include <stdlib.h>

/* STLIB */
#include <string.h>

/* SJLIB */
#include <setjmp.h>

/* MALLOCLIB */
#ifdef OSX
#include <stdlib.h>
#elif LINUX
#include <malloc.h>
#endif

/* MATHLIB */
#include <math.h>

/* Q'Nial header files */

#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"

#include "utils.h"           /* for toreal */
#include "if.h"              /* for checksignal */
#include "ops.h"             /* for simple and splitfb */


static int  Gausselim(double *a, double *b, int n, int p, char *errmsg);


/* isolve implements the Nial primitive that solves linear equations Ax = b.
   It assumes A is a square numeric matrix and that b is a numeric vector with 
   as many elements as rows in A. (b can also be an n x m matrix, and each column 
   is solved in turn).
   The result is a real vector (or matrix).
*/

void
isolve()
{
  nialptr     z,
              a,
              aa = Null,
              b,
              x = Null;
  int         va,
              vb,
              afreed,
              bfreed;
  nialint     n,
              nrhs;
  char        errmsg[80];

  z = apop();
  if (tally(z) != 2) {
    buildfault("arg to solve not a pair");
    freeup(z);
    return;
  }
  splitfb(z, &a, &b);
  va = valence(a);
  vb = valence(b);
  if (va != 2 || vb > 2) {
    buildfault("incorrect valence in solve");
    freeup(a);
    freeup(b);
    freeup(z);
    return;
  }
  n = pickshape(a, 0);
  if (n != pickshape(a, 1) || n != pickshape(b, 0)) {
    buildfault("shapes do not conform in solve");
    freeup(a);
    freeup(b);
    freeup(z);
    return;
  }
  bfreed = false;
  switch (kind(b)) {
    case booltype:
        x = bool_to_real(b);
        bfreed = true;
        break;
    case inttype:
        x = int_to_real(b);
        bfreed = true;
        break;
    case realtype:
        { /* make a copy of b in x */
          x = new_create_array(realtype, vb, 0, shpptr(b, vb));
          copy(x, 0, b, 0, tally(b));
          break;
        }
    case chartype:
    case phrasetype:
    case faulttype:
        {
          buildfault("second arg not numeric type in solve");
          freeup(a);
          freeup(b);
          freeup(z);
          return;
        }
    case atype:
        {
          nialint     i = 0;

          if (!simple(b)) {
            buildfault("second arg not simple in solve");
            freeup(a);
            freeup(b);
            freeup(z);
            return;
          }
          while (i < tally(b)) {
            if (!numeric(kind(fetch_array(b, i)))) {
              buildfault("second arg not numeric type in solve");
              freeup(a);
              freeup(b);
              freeup(z);
              return;
            }
            i++;
          }
          x = to_real(b);  /* makes a copy of b as reals */
        }
  }
  afreed = false;
  switch (kind(a)) {
    case booltype:
        aa = bool_to_real(a); /* makes a copy of a as reals */
        afreed = true;
        break;
    case inttype:
        aa = int_to_real(a); /* makes a copy of a as reals */
        afreed = true;
        break;
    case realtype:
        { /* makes a copy of a */
          aa = new_create_array(realtype, va, 0, shpptr(a, va));
          copy(aa, 0, a, 0, tally(a));
          break;
        }
    case chartype:
    case phrasetype:
    case faulttype:
        {
          buildfault("first arg not numeric type in solve");
          freeup(a);
          if (!bfreed)
            freeup(b);
          freeup(z);
          freeup(x);
          return;
        }
    case atype:
        {
          nialint     i = 0;

          if (!simple(a)) {
            buildfault("first arg not simple in solve");
            freeup(a);
            if (!bfreed)
              freeup(b);
            freeup(z);
            freeup(x);
            return;
          }
          while (i < tally(a)) {
            if (!numeric(kind(fetch_array(a, i)))) {
              buildfault("first arg not numeric type in solve");
              freeup(a);
              if (!bfreed)
                freeup(b);
              freeup(z);
              freeup(x);
              return;
            }
            i++;
          }
          aa = to_real(a); /* makes a copy of a as reals */
        }
  }


  nrhs = (vb == 1 ? 1 : pickshape(x, 1));

  /* use the Gaussian elimination algorithm to solve the equation(s) */
  if (Gausselim(pfirstreal(aa), pfirstreal(x), n, nrhs, errmsg)) {
    apush(x);
  }
  else {
    buildfault(errmsg);
    freeup(x);
  }
  freeup(aa);                /* the modified copy of a has to be freed */
  if (!bfreed)
    freeup(b);
  if (!afreed)
    freeup(a);
  freeup(z);
}

/* the routine iinverse implements the Nial primitive inverse
   that does matrix inversion A.  It produces the matrix X, where AX = I 
   The primitive assumes X is a square numeric matrix.
   The result is a real matrix.
*/


void
iinverse()
{
  nialptr     a,
              aa = Null,
              x;
  int         va,
              afreed;
  nialint     n,
              i,
              j;
  double     *ptr;
  char        errmsg[80];

  a = apop();
  va = valence(a);
  if (va != 2) {
    buildfault("incorrect valence in inverse");
    freeup(a);
    return;
  }
  n = pickshape(a, 0);
  if (n != pickshape(a, 1)) {
    buildfault("matrix is not square in inverse");
    freeup(a);
    return;
  }
  afreed = false;
  switch (kind(a)) {
    case booltype:
        aa = bool_to_real(a);  /* makes a real copy of a */
        afreed = true;
        break;
    case inttype:
        aa = int_to_real(a);  /* makes a real copy of a */

        afreed = true;
        break;
    case realtype:
        {  /* make a real copy of a */
          aa = new_create_array(realtype, va, 0, shpptr(a, va));
          copy(aa, 0, a, 0, tally(a));
          break;
        }
    case chartype:
    case phrasetype:
    case faulttype:
        {
          buildfault("arg not numeric type in inverse");
          freeup(a);
          return;
        }
    case atype:
        {
          nialint     i = 0;

          if (!simple(a)) {
            buildfault("arg not simple in inverse");
            freeup(a);
            return;
          }
          while (i < tally(a)) {
            if (!numeric(kind(fetch_array(a, i)))) {
              buildfault("arg not numeric type in inverse");
              freeup(a);
              return;
            }
            i++;
          }
          aa = to_real(a);  /* makes a real copy of a */

        }
  }
  /* initialize x to a real identity matrix */
  x = new_create_array(realtype, va, 0, shpptr(aa, va));
  ptr = pfirstreal(x);
  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++)
      *ptr++ = (i == j ? 1.0 : 0.0);

  if (Gausselim(pfirstreal(aa), pfirstreal(x), n, n, errmsg)) {
    apush(x);
  }
  else {
    buildfault(errmsg);
    freeup(x);
  }
  freeup(aa);                /* the modified copy of a has to be freed */
  /* if a direct copies of a was made, then it needs to be freed up. the
   * conversion routine does its own freeup. */
  if (!afreed)
    freeup(a);
}


/* the following routine implements the Gaussian Elimination
   algorithm. It can be used with any number of right hand sides.
   Solve calls it with  1 if b is a vector and with m if b is an n x m matrix.
   Inverse calls it with n.
   The code destroys the array arguments, which are known to be
   temporary.

   This code could be replaced by a library routine that might have
   better numerical performance. This code does no scaling.
*/


#define tol 1e-15

static int
Gausselim(double *a, double *b, int n, int p, char *errmsg)
{
  int         i,
              i1,
              j,
              max,
              ii,
              k,
              kk,
              maxk;
  double      temp,
              norm,
              rowsum,
              maxval,
              sum,
              mult;

  /* find infinity norm of matrix a (max of abs row sums) */

  /* the following code walks the matrix in row major order */

  norm = 0.;
  k = 0;
  for (i = 0; i < n; i++) {
    rowsum = 0;
    for (j = 0; j < n; j++)
      rowsum += fabs(*(a + (k++)));
    if (norm < rowsum)
      norm = rowsum;
  }

  /* main loop of elimination */

  for (i = 0; i < n - 1; i++) {
    /* find position of max element in ith column on or below diagonal */

    ii = n * i + i;
    max = i;
    maxval = fabs(*(a + ii));
    for (j = i + 1; j < n; j++) {
      k = n * j + i;
      if (fabs(*(a + k)) > maxval) {
        maxval = fabs(*(a + k));
        max = j;
      }
    }

    /* interchange pivot row and max row if necessary */

    if (max != i) {          /* exchange rows i and max on and to the right
                              * of diagonal */
      k = ii;
      maxk = max * n + i;
      for (j = i; j < n; j++) {
        temp = *(a + k);
        *(a + k) = *(a + maxk);
        *(a + maxk) = temp;
        k++;
        maxk++;
      }

      /* exchange rows i and max of b */
      k = i * p;
      maxk = max * p;
      for (j = 0; j < p; j++) {
        temp = *(b + k);
        *(b + k) = *(b + maxk);
        *(b + maxk) = temp;
        k++;
        maxk++;
      }
    }

    /* test for singularity */
    if (fabs(*(a + ii)) <= (tol * norm))
      goto singular;

    /* compute multipliers and do elimination */
    for (i1 = i + 1; i1 < n; i1++) {
      kk = ii;
      k = i1 * n + i;
      mult = *(a + k) / *(a + kk);
      /* do row reduction on a */
      for (j = i + 1; j < n; j++) {
        k++;
        kk++;
        *(a + k) = *(a + k) - mult * *(a + kk);
      }
      /* do row reduction on b */
      kk = i * p;
      k = i1 * p;
      for (j = 0; j < p; j++) {
        *(b + k) = *(b + k) - mult * *(b + kk);
        k++;
        kk++;
      }
    }
    checksignal(NC_CS_NORMAL);
  }

  /* test for singularity in last pivot */
  ii = (n - 1) * n + n - 1;
  if (fabs(*(a + ii)) <= (tol * norm)) {
singular:
    strcpy(errmsg, "singular matrix");
    return (false);
  }

  /* backsolve the right hand sides in place */

  for (j = 0; j < p; j++) {
    for (i = n - 1; i >= 0; i--) {
      sum = 0.;
      for (k = i + 1; k < n; k++)
        sum += *(a + (n * i + k)) * *(b + (p * k + j));
      *(b + (p * i + j)) = (*(b + (p * i + j)) - sum) / *(a + (n * i + i));
    }
    checksignal(NC_CS_NORMAL);
  }
  return (true);
}

/* iinnerproduct is the routine that implements the Nial primitive inner product
       a is n by k
       b is k by p
   produces
       x which is n by p, where
  x[i;j] = sum (a[i|] * b[|j])

*/

void
iinnerproduct()
{
  nialptr     z,
              a,
              b,
              x;
  int         va,
              vb,
              vx,
              replicatea,
              replicateb;
  nialint     m,
              n,
              bn,
              p,
              i,
              j,
              k,
              sh[2];

  double      sum,
             *ap,
             *bp,
             *xp;

  z = apop();
  if (tally(z) != 2) {
    buildfault("arg to innerproduct not a pair");
    freeup(z);
    return;
  }
  splitfb(z, &a, &b);
  va = valence(a);
  vb = valence(b);
  if (va > 2 || vb > 2) {
    buildfault("incorrect valence in innerproduct");
    freeup(a);
    freeup(b);
    freeup(z);
    return;
  }
  /* ensure b is realtype */
  switch (kind(b)) {
    case booltype:
        b = bool_to_real(b);
        break;
    case inttype:
        b = int_to_real(b);
        break;
    case realtype:
        break;
    case chartype:
    case phrasetype:
    case faulttype:
        {
          buildfault("second arg not numeric type in innerproduct");
          freeup(a);
          freeup(b);
          freeup(z);
          return;
        }
    case atype:
        {
          nialint     i = 0;

          if (!simple(b)) {
            buildfault("arg not simple in innerproduct");
            freeup(a);
            freeup(b);
            freeup(z);
            return;
          }
          while (i < tally(b)) {
            if (!numeric(kind(fetch_array(b, i)))) {
              buildfault("second arg not numeric type in innerproduct");
              freeup(a);
              freeup(b);
              freeup(z);
              return;
            }
            i++;
          }
          b = to_real(b);    /* safe because freeup(z) will clear old b */
        }
  }
  /* ensure a is realtype */
  switch (kind(a)) {
    case booltype:
        a = bool_to_real(a);
        break;
    case inttype:
        a = int_to_real(a);
        break;
    case realtype:
        break;
    case chartype:
    case phrasetype:
    case faulttype:
        {
          buildfault("first arg not numeric type in innerproduct");
          freeup(a);
          freeup(b);
          freeup(z);
          return;
        }
    case atype:
        {
          nialint     i = 0;

          if (!simple(a)) {
            buildfault("first arg not simple in innerproduct");
            freeup(a);
            freeup(b);
            freeup(z);
            return;
          }
          while (i < tally(a)) {
            if (!numeric(kind(fetch_array(a, i)))) {
              buildfault("first arg not numeric type in innerproduct");
              freeup(a);
              freeup(b);
              freeup(z);
              return;
            }
            i++;
          }
          a = to_real(a);    /* safe becasue freeup of z clears old a */
        }
  }


  if (va == 0) {
    m = 1;
    n = 1;
  }
  else if (va == 1) {
    m = 1;
    n = pickshape(a, 0);
  }
  else {
    m = pickshape(a, 0);
    n = pickshape(a, 1);
  }

  if (vb == 0) {
    bn = 1;
    p = 1;
  }
  else if (vb == 1) {
    bn = pickshape(b, 0);
    p = 1;
  }
  else {
    bn = pickshape(b, 0);
    p = pickshape(b, 1);
  }

  replicatea = (va == 0);
  replicateb = (vb == 0);

  if (!(replicatea || replicateb) && n != bn) {
    buildfault("conform error in innerproduct");
    freeup(a);
    freeup(b);
    freeup(z);
    return;
  }

  /* get valence for the result */
  vx = (va <= 1 ? (vb <= 1 ? 0 : 1) : (vb <= 1 ? 1 : 2));
  if (vx == 2) {
    sh[0] = m;
    sh[1] = p;
  }
  else if (vx == 1)
    sh[0] = (va == 2 ? m : p);
  /* if vx==0 then sh is not used */

  /* allocate space for the result matrix */
  x = new_create_array(realtype, vx, 0, sh);

  ap = pfirstreal(a);        /* safe: no allocations */
  bp = pfirstreal(b);        /* safe: no allocations */
  xp = pfirstreal(x);        /* safe: no allocations */

  /* type of loop chosen on the kind of ip being done */

  if (vx == 2) {             /* matrix - matrix */
    for (i = 0; i < m; i++) {
      for (j = 0; j < p; j++) {
        sum = 0.;
        for (k = 0; k < n; k++)
          sum += *(ap + (n * i + k)) * *(bp + (p * k + j));
        *(xp + (p * i + j)) = sum;
      }
      checksignal(NC_CS_NORMAL);
    }
  }
  else if (va == 2) {        /* matrix - vector */
    for (i = 0; i < m; i++) {
      sum = 0.;
      if (replicateb)
        for (k = 0; k < n; k++)
          sum += *(ap + (n * i + k)) * *bp;
      else
        for (k = 0; k < n; k++)
          sum += *(ap + (n * i + k)) * *(bp + k);
      *(xp + i) = sum;
    }
  }
  else if (vb == 2) {        /* vector - matrix */
    for (j = 0; j < p; j++) {
      sum = 0.;
      if (replicatea)
        for (k = 0; k < bn; k++)
          sum += *ap * *(bp + (p * k + j));
      else
        for (k = 0; k < bn; k++)
          sum += *(ap + k) * *(bp + (p * k + j));
      *(xp + j) = sum;
    }
  }
  else {                     /* vector - vector */
    sum = 0.;
    if (replicatea)
      for (k = 0; k < bn; k++)
        sum += *ap * *(bp + k);
    else if (replicateb)
      for (k = 0; k < n; k++)
        sum += *(ap + k) * *bp;
    else
      for (k = 0; k < n; k++)
        sum += *(ap + k) * *(bp + k);
    *xp = sum;
  }

  apush(x);
  freeup(a);
  freeup(b);
  freeup(z);
}
