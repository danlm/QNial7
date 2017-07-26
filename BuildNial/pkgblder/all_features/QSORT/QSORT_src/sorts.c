/*==============================================================

  MODULE   SORTS.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This module contains various sorting algorithms implemented
  as primitives in 

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
#include "sorts.h"
#include "trs.h"             /* for int_each etc. */
#include "utils.h"           /* for converters */
#include "faults.h"          /* for logical fault */
#include "logicops.h"        /* for orbools and andbools */
#include "ops.h"             /* for simple and splifb */


#ifdef QSORT /* ( */

/**
 * This is an implementation of QSort as detailed in Sedgewick. The code
 * uses an insertion sort for small partitions, implements a median of 3 
 * pivot selection and is non-recursive.
 *
 * There is a single nial primitive which then selects a type based
 * implementation for speed.
 *
 */


/* Size at which we swap to insertion sort */ 
#define USE_ISORT_SIZE 16


/* ------------------ Insertion Sorting ---------------------- */

/**
 * Insertion sort for integers
 */ 
static void insertion_sort_int(nialint *x, nialint lo, nialint hi) {
  nialint i;

  /* 
   * Put the smallest array element into the first position
   * to act as a sentinel
   */
  for (i = hi; i > lo; i--)
    if (x[i] < x[i-1]) {
      nialint t1 = x[i-1];
      x[i-1] = x[i];
      x[i] = t1;
    }

  for (i = lo+2; i <= hi; i++) {
    nialint j = i;
    nialint v = x[i];

    while (v < x[j-1]) {
      x[j] = x[j-1];
      j--;
    }
    
    x[j] = v;
  }

  return;
}


/**
 * Insertion sort for reals
 */ 
static void insertion_sort_real(double *x, nialint lo, nialint hi) {
  nialint i;

  /* 
   * Put the smallest array element into the first position
   * to act as a sentinel
   */
  for (i = hi; i > lo; i--)
    if (x[i] < x[i-1]) {
      double t1 = x[i-1];
      x[i-1] = x[i];
      x[i] = t1;
    }

  for (i = lo+2; i <= hi; i++) {
    nialint j = i;
    double v = x[i];

    while (v < x[j-1]) {
      x[j] = x[j-1];
      j--;
    }
    
    x[j] = v;
  }

  return;
}


/**
 * Insertion sort for chars
 */ 
static void insertion_sort_char(unsigned char *x, nialint lo, nialint hi) {
  nialint i;

  /* 
   * Put the smallest array element into the first position
   * to act as a sentinel
   */
  for (i = hi; i > lo; i--)
    if (x[i] < x[i-1]) {
      unsigned char t1 = x[i-1];
      x[i-1] = x[i];
      x[i] = t1;
    }

  for (i = lo+2; i <= hi; i++) {
    nialint j = i;
    unsigned char v = x[i];

    while (v < x[j-1]) {
      x[j] = x[j-1];
      j--;
    }
    
    x[j] = v;
  }

  return;
}


/**
 * Insertion sort for integers
 */ 
static void insertion_sort_gen(nialptr *x, nialint lo, nialint hi) {
  nialint i;

  /* 
   * Put the smallest array element into the first position
   * to act as a sentinel
   */
  for (i = hi; i > lo; i--)
    if (up(x[i], x[i-1])) {
      nialptr t1 = x[i-1];
      x[i-1] = x[i];
      x[i] = t1;
    }

  for (i = lo+2; i <= hi; i++) {
    nialint j = i;
    nialptr v = x[i];

    while (up(v, x[j-1])) {
      x[j] = x[j-1];
      j--;
    }
    
    x[j] = v;
  }

  return;
}



/* ------------------------ Quick Sort ------------------------*/


/* --------------------- Integers ----------------------- */

/**
 * Rearrange x into two partitions and return the 
 * boundary. The pivot is the last element of the array.
 *
 */
static int qpartition_int(nialint *x, nialint l, nialint r) {
  nialint i = l-1;
  nialint j = r;
  nialint v = x[r];
  nialint t1;

  /* partition the data */
  for(;;) {
    while (x[++i] < v)
      ;
    while (v < x[--j])
      if (j == l)
        break;

    if (i >= j) 
      break;

    /* exchange x[i] and x[j] */
    t1 = x[i];
    x[i] = x[j];
    x[j] = t1;

  }

  /* exchange x[i] and x[r] */
  t1 = x[i];
  x[i] = x[r];
  x[r] = t1;
    
  return i;
}


/**
 * Perform the quicksort of the provided array of integers.
 * This is done without recursion using a stack.
 *
 * After partitioning we handle the smallest partition first 
 * and save the larger partition details on the stack. This
 * means that the stack size will never exceed log2(N). 
 *  
 */
static void qsort_int(nialint *x, nialint lo, nialint hi) {
  nialint i;
  nialint stack[128], *sptr = stack; /* sptr == stack means empty stack */

  for(;;) {
    
    /* 
     * Handle special cases of which there are two types.
     *
     * 1. We sort a small partition using insertion sort (lo < hi)
     * 2. There is nothing to do (lo >= hi)
     * 
     * In either case we load the next partition from the 
     * stack unless we are are finished.
     */
    if (hi - lo < USE_ISORT_SIZE) {
      if (lo < hi) {
        /* use insertion sort */
        insertion_sort_int(x, lo, hi);
      }
      
      /* Is there is not another partition return */
      if (sptr == stack)
        return;
      
      /* Load up the next partition */
      hi = *sptr--;
      lo = *sptr--;

    } else {
      /* Restructure the partition for median of 3 pivot */
      nialint m = (lo+hi)/2;
      nialint val_t, val_lo = x[lo], val_hi = x[hi], val_m = x[m];

      /* 
       * we will arrange the 3 values in correct order with 
       * the chosen pivot second from the high end. Since
       * the partition code uses the end value as pivot we do not 
       * then need to include the end values in the partition as
       * they are already pre-partitioned
       */

      x[m] = x[hi-1];    /* swap with median */
      
      /* put hi and lo vals in correct order */
      if (val_lo > val_hi) {
        val_t = val_hi;
        val_hi = val_lo;
        val_lo = val_t;
      }

      /* put middle value in correct order */
      if (val_m < val_lo) {
        val_t = val_lo;
        val_lo = val_m;
        val_m = val_t;
      } else if (val_m > val_hi) {
        val_t = val_hi;
        val_hi = val_m;
        val_m = val_t;
      }

      /* Put arranged values back in list */
      x[lo] = val_lo;
      x[hi] = val_hi;
      x[hi-1] = val_m;
        
    
      /* 
       * Perform standard partitioning ignoring the end values as 
       * they are already partitioned.
       */
      i = qpartition_int(x, lo+1, hi-1);
      
      /* Arrange to sort smallest partition first */
      if (i-lo > hi-i) {
        /* First partition is largest, save on stack */
        *++sptr = lo;
        *++sptr = i-1;

        lo = i+1;   /* Reset lo, hi is unchanged */
      } else {
        /* First partition is smallest, save second on stack */
        *++sptr = i+1; 
        *++sptr = hi;

        hi = i-1;   /* Reset hi, lo is unchanged */
      }
    }
  }

  /* Should never get here */
  return;
}



/**
 * Quick sort of an array of integers
 */
static nialptr n_qsort_int(nialptr x, nialint t) {
  nialptr res;
  nialint v = valence(x);
  nialint i, *p, *q;

  /* copy the data to a new array */
  res = new_create_array(inttype, valence(x), 0, shpptr(x,v));
  for (i = 0, p = pfirstint(res), q = pfirstint(x); i < t; i++)
    *p++ = *q++;

  if (t > 1) 
    qsort_int(pfirstint(res), 0, t-1);

  return res;
}


/* ------------------------ Reals ----------------------- */


/**
 * Rearrange x into two partitions and return the 
 * boundary.
 * (Sedgewick)
 */
static int qpartition_real(double *x, nialint l, nialint r) {
  nialint i = l-1;
  nialint j = r;
  double v = x[r];
  double t1;

  /* partition the data */
  for(;;) {
    while (x[++i] < v)
      ;
    while (v < x[--j])
      if (j == l)
        break;

    if (i >= j) 
      break;

    /* exchange x[i] and x[j] */
    t1 = x[i];
    x[i] = x[j];
    x[j] = t1;

  }

  /* exchange x[i] and x[r] */
  t1 = x[i];
  x[i] = x[r];
  x[r] = t1;
    
  return i;
}

 
/**
 * Perform the quicksort of the provided array of reals.
 * This is done without recursion using a stack.
 *
 * After partitioning we handle the smallest partition first 
 * and save the larger partition details on the stack. This
 * means that the stack size will never exceed log2(N). 
 *  
 */
static void qsort_real(double *x, nialint lo, nialint hi) {
  nialint i;
  nialint stack[128], *sptr = stack; /* sptr == stack means empty stack */

  for(;;) {
    
    /* 
     * Handle special cases of which there are two types.
     *
     * 1. We sort a small partition using insertion sort (lo < hi)
     * 2. There is nothing to do (lo >= hi)
     * 
     * In either case we load the next partition from the 
     * stack unless we are are finished.
     */
    if (hi - lo < USE_ISORT_SIZE) {
      if (lo < hi) {
        /* use insertion sort */
        insertion_sort_real(x, lo, hi);
      }
      
      /* Is there is not another partition return */
      if (sptr == stack)
        return;
      
      /* Load up the next partition */
      hi = *sptr--;
      lo = *sptr--;

    } else {
      /* Restructure the partition for median of 3 pivot */
      nialint m = (lo+hi)/2;
      double val_t, val_lo = x[lo], val_hi = x[hi], val_m = x[m];

      /* 
       * we will arrange the 3 values in correct order with 
       * the chosen pivot second from the high end. Since
       * the partition code uses the end value as pivot we do not 
       * then need to include the end values in the partition as
       * they are already pre-partitioned
       */

      x[m] = x[hi-1];    /* swap with median */
      
      /* put hi and lo vals in correct order */
      if (val_lo > val_hi) {
        val_t = val_hi;
        val_hi = val_lo;
        val_lo = val_t;
      }

      /* put middle value in correct order */
      if (val_m < val_lo) {
        val_t = val_lo;
        val_lo = val_m;
        val_m = val_t;
      } else if (val_m > val_hi) {
        val_t = val_hi;
        val_hi = val_m;
        val_m = val_t;
      }

      /* Put arranged values back in list */
      x[lo] = val_lo;
      x[hi] = val_hi;
      x[hi-1] = val_m;
        
    
      /* 
       * Perform standard partitioning ignoring the end values as 
       * they are already partitioned.
       */
      i = qpartition_real(x, lo+1, hi-1);
      
      /* Arrange to sort smallest partition first */
      if (i-lo > hi-i) {
        /* First partition is largest, save on stack */
        *++sptr = lo;
        *++sptr = i-1;

        lo = i+1;   /* Reset lo, hi is unchanged */
      } else {
        /* First partition is smallest, save second on stack */
        *++sptr = i+1; 
        *++sptr = hi;

        hi = i-1;   /* Reset hi, lo is unchanged */
      }
    }
  }

  /* Should never get here */
  return;
}


/**
 * Quick sort of an array of reals
 */
static nialptr n_qsort_real(nialptr x, nialint t) {
  nialptr res;
  nialint v = valence(x);
  nialint i;
  double *p, *q;

  /* copy the data to a new array */
  res = new_create_array(realtype, valence(x), 0, shpptr(x,v));
  for (i = 0, p = pfirstreal(res), q = pfirstreal(x); i < t; i++)
    *p++ = *q++;

  if (t > 1) 
    qsort_real(pfirstreal(res), 0, t-1);

  return res;
}


/* -------------------- Characters -------------------------- */


/**
 * Rearrange x into two partitions and return the 
 * boundary. The pivot is the last element of the array.
 *
 */
static int qpartition_char(unsigned char *x, nialint l, nialint r) {
  nialint i = l-1;
  nialint j = r;
  unsigned char v = x[r];
  unsigned char t1;

  /* partition the data */
  for(;;) {
    while (x[++i] < v)
      ;
    while (v < x[--j])
      if (j == l)
        break;

    if (i >= j) 
      break;

    /* exchange x[i] and x[j] */
    t1 = x[i];
    x[i] = x[j];
    x[j] = t1;

  }

  /* exchange x[i] and x[r] */
  t1 = x[i];
  x[i] = x[r];
  x[r] = t1;
    
  return i;
}



/**
 * Perform the quicksort of the provided array of chars.
 * This is done without recursion using a stack.
 *
 * After partitioning we handle the smallest partition first 
 * and save the larger partition details on the stack. This
 * means that the stack size will never exceed log2(N). 
 *  
 */
static void qsort_char(unsigned char *x, nialint lo, nialint hi) {
  nialint i;
  nialint stack[128], *sptr = stack; /* sptr == stack means empty stack */

  for(;;) {
    
    /* 
     * Handle special cases of which there are two types.
     *
     * 1. We sort a small partition using insertion sort (lo < hi)
     * 2. There is nothing to do (lo >= hi)
     * 
     * In either case we load the next partition from the 
     * stack unless we are are finished.
     */
    if (hi - lo < USE_ISORT_SIZE) {
      if (lo < hi) {
        /* use insertion sort */
        insertion_sort_char(x, lo, hi);
      }
      
      /* Is there is not another partition return */
      if (sptr == stack)
        return;
      
      /* Load up the next partition */
      hi = *sptr--;
      lo = *sptr--;

    } else {
      /* Restructure the partition for median of 3 pivot */
      nialint m = (lo+hi)/2;
      unsigned char val_t, val_lo = x[lo], val_hi = x[hi], val_m = x[m];

      /* 
       * we will arrange the 3 values in correct order with 
       * the chosen pivot second from the high end. Since
       * the partition code uses the end value as pivot we do not 
       * then need to include the end values in the partition as
       * they are already pre-partitioned
       */

      x[m] = x[hi-1];    /* swap with median */
      
      /* put hi and lo vals in correct order */
      if (val_lo > val_hi) {
        val_t = val_hi;
        val_hi = val_lo;
        val_lo = val_t;
      }

      /* put middle value in correct order */
      if (val_m < val_lo) {
        val_t = val_lo;
        val_lo = val_m;
        val_m = val_t;
      } else if (val_m > val_hi) {
        val_t = val_hi;
        val_hi = val_m;
        val_m = val_t;
      }

      /* Put arranged values back in list */
      x[lo] = val_lo;
      x[hi] = val_hi;
      x[hi-1] = val_m;
        
    
      /* 
       * Perform standard partitioning ignoring the end values as 
       * they are already partitioned.
       */
      i = qpartition_char(x, lo+1, hi-1);
      
      /* Arrange to sort smallest partition first */
      if (i-lo > hi-i) {
        /* First partition is largest, save on stack */
        *++sptr = lo;
        *++sptr = i-1;

        lo = i+1;   /* Reset lo, hi is unchanged */
      } else {
        /* First partition is smallest, save second on stack */
        *++sptr = i+1; 
        *++sptr = hi;

        hi = i-1;   /* Reset hi, lo is unchanged */
      }
    }
  }

  /* Should never get here */
  return;
}



/**
 * Quick sort of an array of characters
 */
static nialptr n_qsort_char(nialptr x, nialint t) {
  nialptr res;
  nialint v = valence(x);
  nialint i;
  unsigned char *p, *q;

  /* copy the data to a new array */
  res = new_create_array(chartype, valence(x), 0, shpptr(x,v));
  for (i = 0, p = (unsigned char *)pfirstchar(res), q = (unsigned char *)pfirstchar(x); i < t; i++)
    *p++ = *q++;

  if (t > 1) 
    qsort_char((unsigned char *)pfirstchar(res), 0, t-1);

  return res;
}

/* -------------------- Generic -------------------------- */


/**
 * Rearrange x into two partitions and return the 
 * boundary. The pivot is the last element of the array.
 *
 */
static int qpartition_gen(nialptr *x, nialint l, nialint r) {
  nialint i = l-1;
  nialint j = r;
  nialptr v = x[r];
  nialptr t1;

  /* partition the data */
  for(;;) {
    while (up(x[++i], v))
      ;
    while (up(v, x[--j]))
      if (j == l)
        break;

    if (i >= j) 
      break;

    /* exchange x[i] and x[j] */
    t1 = x[i];
    x[i] = x[j];
    x[j] = t1;

  }

  /* exchange x[i] and x[r] */
  t1 = x[i];
  x[i] = x[r];
  x[r] = t1;
    
  return i;
}



/**
 * Perform the quicksort of the provided array of pointers.
 * This is done without recursion using a stack.
 *
 * After partitioning we handle the smallest partition first 
 * and save the larger partition details on the stack. This
 * means that the stack size will never exceed log2(N). 
 *  
 */
static void qsort_gen(nialptr *x, nialint lo, nialint hi) {
  nialint i;
  nialint stack[128], *sptr = stack; /* sptr == stack means empty stack */

  for(;;) {
    
    /* 
     * Handle special cases of which there are two types.
     *
     * 1. We sort a small partition using insertion sort (lo < hi)
     * 2. There is nothing to do (lo >= hi)
     * 
     * In either case we load the next partition from the 
     * stack unless we are are finished.
     */
    if (hi - lo < USE_ISORT_SIZE) {
      if (lo < hi) {
        /* use insertion sort */
        insertion_sort_gen(x, lo, hi);
      }
      
      /* Is there is not another partition return */
      if (sptr == stack)
        return;
      
      /* Load up the next partition */
      hi = *sptr--;
      lo = *sptr--;

    } else {
      /* Restructure the partition for median of 3 pivot */
      nialint m = (lo+hi)/2;
      unsigned char val_t, val_lo = x[lo], val_hi = x[hi], val_m = x[m];

      /* 
       * we will arrange the 3 values in correct order with 
       * the chosen pivot second from the high end. Since
       * the partition code uses the end value as pivot we do not 
       * then need to include the end values in the partition as
       * they are already pre-partitioned
       */

      x[m] = x[hi-1];    /* swap with median */
      
      /* put hi and lo vals in correct order */
      if (up(val_lo, val_hi)) {
        val_t = val_hi;
        val_hi = val_lo;
        val_lo = val_t;
      }

      /* put middle value in correct order */
      if (up(val_m, val_lo)) {
        val_t = val_lo;
        val_lo = val_m;
        val_m = val_t;
      } else if (up(val_hi, val_m)) {
        val_t = val_hi;
        val_hi = val_m;
        val_m = val_t;
      }

      /* Put arranged values back in list */
      x[lo] = val_lo;
      x[hi] = val_hi;
      x[hi-1] = val_m;
        
    
      /* 
       * Perform standard partitioning ignoring the end values as 
       * they are already partitioned.
       */
      i = qpartition_gen(x, lo+1, hi-1);
      
      /* Arrange to sort smallest partition first */
      if (i-lo > hi-i) {
        /* First partition is largest, save on stack */
        *++sptr = lo;
        *++sptr = i-1;

        lo = i+1;   /* Reset lo, hi is unchanged */
      } else {
        /* First partition is smallest, save second on stack */
        *++sptr = i+1; 
        *++sptr = hi;

        hi = i-1;   /* Reset hi, lo is unchanged */
      }
    }
  }

  /* Should never get here */
  return;
}



/**
 * Quick sort of an array of pointers
 */
static nialptr n_qsort_gen(nialptr x, nialint t) {
  nialptr res;
  nialint v = valence(x);
  nialint i;

  /* copy the data to a new array */
  res = new_create_array(atype, valence(x), 0, shpptr(x,v));
  for (i = 0; i < t; i++)
    store_array(res, i, fetch_array(x, i));

  if (t > 1) 
    qsort_gen((nialptr*)pfirstitem(res), 0, t-1);

  return res;
}


/* --------------------- Nial Interface ---------------------- */
/**
 * Primary entry point to sorting
 */
void iqsort(void) {
  nialptr x = apop();
  nialint t = tally(x);
  nialint k = kind(x);

  switch(k) {
  case inttype:
    apush(n_qsort_int(x, t));
    break;

  case realtype:
    apush(n_qsort_real(x, t));
    break;

  case chartype:
    apush(n_qsort_char(x,t));
    break;

  case atype:
    apush(n_qsort_gen(x,t));
    break;

  default:
    apush(makefault("invalid qsort type"));
    break;
  }

  freeup(x);
  return;
}


#endif /* QSORT ) */
