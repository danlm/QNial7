/*==============================================================

  MODULE   NIALDSP.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  Support functions for Digital Signal Processing.
  
================================================================*/


/* Q'Nial file that selects features */

#include "switches.h"

#ifdef NIALDSP

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

/* SIGLIB */
#include <signal.h>

/* SJLIB */
#include <setjmp.h>

/* PROCESSLIB */
#include <sys/types.h>
#ifdef UNIXSYS
#include <pwd.h>
#endif
#include <fcntl.h>
#ifdef UNIXSYS
#include <netdb.h>
#endif
#include <errno.h>

/* Q'Nial header files */

#include "absmach.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"
#include "blders.h"

#ifdef UNIXSYS
#include "unixif.h"
#endif
#ifdef WINNIAL
#include "windowsif.h"
#endif

#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>

#include <math.h>


/**
 * Y = _dspFilter A B X
 * 
 * Implements the finite difference equation for filters.
 *
 *  This takes the form -
 *
 *     a(1)y(n) = b(1)x(n) + b(2)x(n-1) + ... + b(Nb)x(n-Nb+1)
 *                -a(2)y(n-1) - ... - a(Na)y(n-Na+1)
 *
 *  The implementation normalises the A and B vectors by dividing
 *  by a(0).
 *  
 *  The first part is the convolution transform with impulse
 *  response B/a(0). The second is a feedback computation.
 *
 *  If tally A is 1 then there is no feedback and the
 *  second part is avoided.
 *
 *  If tally B is 0 then the first part is avoided. This is to
 *  allow the first part to be replaced by an FFT convolution.
 *  
 */
void idsp_filter(void) {
   nialptr x = apop();
   nialptr a, b, d;
   nialint i;
   nialptr res, work;
   nialint worklen, reslen, clen;
   double *wptr, *cptr, *dptr;
   double *p, *q, *r;
   double a0;
   
   if (kind(x) != atype || tally(x) != 3) {
    apush(makefault("?args"));
    freeup(x);
    return;
   }
   
   /* Extract the 3 arguments */
   a = fetch_array(x, 0);
   b = fetch_array(x, 1);
   d = fetch_array(x, 2);
   
   /* Check validity */
   if (kind(a) != realtype || (kind(b) != realtype && tally(b) > 0) || kind(d) != realtype) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;    
   }
   
   /*
    * We create a work array divided into 3 zones.
    *   Zone 1 is for the coefficient arrays
    *   Zone 2 is a zero padded area for non-existent x(n) or y(n)
    *   Zone 3 holds X  or Y in the computation
    */
   
   reslen  = tally(d);
   clen    = (tally(a) > tally(b))? tally(a): tally(b);
   worklen = reslen + 2*clen;
   work    = new_create_array(realtype, 1, 0, &worklen);
   res     = new_create_array(realtype, valence(d), 0, shpptr(d, valence(d)));

   /* Set up zone pointers */
   cptr = pfirstreal(work);     /* Coefficients */
   wptr = cptr + clen;          /* Padding      */
   dptr = wptr + clen;          /* Data         */

   /* Copy the supplied X into the data area */
   memcpy(dptr, pfirstreal(d), tally(d)*sizeof(double));

   /* Get the normalising coefficient */
   a0 = fetch_real(a, 0);

   /* Initialise the padding */
   for (i = 0, p = wptr; i < clen; i++)
      *p++ = 0.0;
   
   /* Avoid the impulse response convolution if B is Null */
   if (tally(b) == 0) {
      /*
      * Copy the X values directly to the output to set up for
      * the second pass
      */
      memcpy(pfirstreal(res), pfirstreal(d), tally(d)*sizeof(double));
      goto second_pass;
   }
     
     
   /* ----------- Convolution involving x and b ---------------------- */

   /*
   * Flip the values of the normalised B vector
   * into the coefficient area
   */   
   for (i = 0, p = cptr + tally(b), q = pfirstreal(b); i < tally(b); i++)
      *--p = *q++/a0;
   
   /* Perform the convolution */
   r = pfirstreal(res);
   wptr = dptr - tally(b) + 1;
   for (i = 0; i < tally(d); i++) {
        nialint j;
        double y_n = 0.0;
        p = wptr;
        q = cptr;
        for (j = 0; j < tally(b); j++) {
            y_n += (*p++)*(*q++);
        }
        
        /* store the output in the Y vector and slide the input signal */
        *r++ = y_n;
        wptr++;
   }

   /* Avoid second pass if not required */   
   if (tally(a) == 1)
      goto return_result;
    
second_pass:

   /*
   * The second pass is a feedback computation involving Y and A (without a(0))
   */

   /* Copy the Y values back into the work zone for feedback */ 
   memcpy(dptr, pfirstreal(res), tally(d)*sizeof(double));
   
   /* 
    * Flip the normalised A coefficients into the coefficient area.
    * We only collect coefficients 
    */
   for (i = 0, p = cptr + tally(a), q = pfirstreal(a); i < tally(a); i++)
      *--p = *q++/a0;
   
   /* Compute Y values with feedback */
   r = pfirstreal(res);
   wptr = dptr - tally(a) + 2;
   
   for (i = 0; i < tally(d) - 1; i++) {
        nialint j;
        double y_n = *++r;      /* Initial y(n) value */
        p = wptr;
        q = cptr;
        for (j = 0; j < tally(a) - 1; j++) {
            y_n -= (*p++)*(*q++);
        }
        
        /* Store y(n) into output */
        *r = y_n;
        
        /* Update work area with new y(n) for feedback */
        *p = y_n;
        
        /* Slide the output up */
        wptr++;
   }
   
return_result:

   freeup(work);
   apush(res);
   freeup(x);
   return;
}


/**
 * idspfilter2 is identical to idspfilter with the exception that it
 * compacts the A coefficients to eliminate zeroes and reduce the
 * overheads of computing the recurrence relation. In doing this
 * it adds a small overhead on pointer modification.
 */

void idsp_filter2(void) {
   nialptr x = apop();
   nialptr a, b, d;
   nialint i, j, ind, *iptr;
   nialptr res, work, indexes;
   nialint worklen, reslen, clen;
   double *wptr, *cptr, *dptr;
   double *p, *q, *r;
   double a0, last_coeff;  
   
   if (kind(x) != atype || tally(x) != 3) {
    apush(makefault("?args"));
    freeup(x);
    return;
   }
   
   /* Extract the 3 arguments */
   a = fetch_array(x, 0);
   b = fetch_array(x, 1);
   d = fetch_array(x, 2);
   
   /* Check validity */
   if (kind(a) != realtype || (kind(b) != realtype && tally(b) > 0) || kind(d) != realtype) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;    
   }
   
   /*
    * We create a work array divided into 3 zones.
    *   Zone 1 is for the coefficient arrays
    *   Zone 2 is a zero padded area for non-existent x(n) or y(n)
    *   Zone 3 holds X  or Y in the computation
    */
   
   reslen  = tally(d);
   clen    = (tally(a) > tally(b))? tally(a): tally(b);
   worklen = reslen + 2*clen;
   indexes = new_create_array(inttype, 1, 0, &clen);
   work    = new_create_array(realtype, 1, 0, &worklen);
   res     = new_create_array(realtype, valence(d), 0, shpptr(d, valence(d)));

   /* Set up zone pointers */
   cptr = pfirstreal(work);     /* Coefficients */
   wptr = cptr + clen;          /* Padding      */
   dptr = wptr + clen;          /* Data         */
   iptr = pfirstint(indexes);

   /* Copy the supplied X into the data area */
   memcpy(dptr, pfirstreal(d), tally(d)*sizeof(double));

   /* Get the normalising coefficient */
   a0 = fetch_real(a, 0);

   /* Initialise the padding */
   for (i = 0, p = wptr; i < clen; i++)
      *p++ = 0.0;
   
   /* Avoid the impulse response convolution if B is Null */
   if (tally(b) == 0) {
      /*
      * Copy the X values directly to the output to set up for
      * the second pass
      */
      memcpy(pfirstreal(res), pfirstreal(d), tally(d)*sizeof(double));
      goto second_pass;
   }
     
     
   /* ----------- Convolution involving x and b ---------------------- */

   /*
   * Flip the values of the normalised B vector
   * into the coefficient area
   */   
   for (i = 0, p = cptr + tally(b), q = pfirstreal(b); i < tally(b); i++)
      *--p = *q++/a0;
   
   /* Perform the convolution */
   r = pfirstreal(res);
   wptr = dptr - tally(b) + 1;
   for (i = 0; i < tally(d); i++) {
        nialint j;
        double y_n = 0.0;
        p = wptr;
        q = cptr;
        for (j = 0; j < tally(b); j++) {
            y_n += (*p++)*(*q++);
        }
        
        /* store the output in the Y vector and slide the input signal */
        *r++ = y_n;
        wptr++;
   }

   /* Avoid second pass if not required */   
   if (tally(a) == 1)
      goto return_result;
    
second_pass:

   /*
   * The second pass is a feedback computation involving Y and A (without a(0))
   */
   printf("Entering phase 2\n");
   fflush(stdout);

   /* Copy the Y values back into the work zone for feedback */ 
   memcpy(dptr, pfirstreal(res), tally(d)*sizeof(double));
   
   /* 
    * Flip the normalised A coefficients into the coefficient area.
    */
   for (i = 0, p = cptr + tally(a), q = pfirstreal(a); i < tally(a); i++) 
      *--p = *q++/a0;
   
   /* Compact and count the coefficients */
   iptr[0] = 0;
   last_coeff = 1.0;
   for (i = 0, p = cptr, j = 0; i < tally(a) - 1; i++, p++) {
      if (*p != 0) {
            cptr[j] = *p;
            iptr[++j] = 0;
      } else {
         iptr[j] += 1;
      }
   }
      
   
   /* Set ind now to hold the number of compacted coefficients */
   ind = j;
      
   /* Compute Y values with feedback */
   r = pfirstreal(res);
   wptr = dptr - tally(a) + 2;
   
   for (i = 0; i < tally(d) - 1; i++) {
        nialint j;
        double y_n = *++r;      /* Initial y(n) value */
        p = wptr;
        for (j = 0; j < ind; j++) {
             p += iptr[j];
             y_n -= (*p++)*cptr[j];
        }
        
        /* Store y(n) into output */
        *r = y_n;
        
        /* Update work area with new y(n) for feedback */
        wptr[tally(a)-1] = y_n;
        
        /* Slide the output up */
        wptr++;
   }
   
return_result:

   freeup(work);
   freeup(indexes);
   apush(res);
   freeup(x);
   return;
}

#endif /* NIALDSP */

