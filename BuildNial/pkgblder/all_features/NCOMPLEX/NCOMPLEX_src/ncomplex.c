/*==============================================================

  MODULE   NCOMPLEX.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  Simple library of complex number routines.
  
  A complex number is represented as a pair of doubles. In this
  the basic operations of addition and subtraction are real
  operations and we only need to handle the cases of division
  and multiplication.
  
  Arrays of complex numbers are arrays where the dimension of the
  last axis is 2.

================================================================*/


/* Q'Nial file that selects features */

#include "switches.h"

#ifdef NCOMPLEX

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


/* Macros for simplicity */

#define VALID_COMPLEX(x) (kind(x) == realtype && valence(x) > 0 && *shpptr(x,1) == 2)


/* ----------------- Internal Operations for speed ----------------------- */



/**
 * Complex division of two arrays of numbers
 */
static void complex_div2(double *dest, double *a, nialint a_len, double *b, nialint b_len) {
    nialint i = 0;
    
    if (a_len == 2) {
        /* Handle as special case */
        register double a_re = *a++, a_im = *a++;
        for (i = 0; i < b_len; i += 2) {
            register double b_re = *b++, b_im = *b++;
            register double base = b_re*b_re + b_im*b_im;
            *dest++ = (a_re*b_re + a_im*b_im)/base;
            *dest++ = (a_im*b_re - a_re*b_im)/base;
        }
        
    } else if (b_len == 2) {
        /* Special case */
        register double b_re = *b++, b_im = *b++;
        register double base = b_re*b_re + b_im*b_im;
        for (i = 0; i < a_len; i += 2) {
            register double a_re = *a++, a_im = *a++;
            *dest++ = (a_re*b_re + a_im*b_im)/base;
            *dest++ = (a_im*b_re - a_re*b_im)/base;           
        }
    } else {
        /* Division of conformant arrays */
        for (i = 0; i < b_len; i += 2) {
            register double a_re = *a++, a_im = *a++;
            register double b_re = *b++, b_im = *b++;
            register double base = b_re*b_re + b_im*b_im;
            *dest++ = (a_re*b_re + a_im*b_im)/base;
            *dest++ = (a_im*b_re - a_re*b_im)/base;
        }
    }
    
    return;
}


/**
 * Complex product of two arrays of numbers
 */
static void complex_prod2(double *dest, double *a, nialint a_len, double *b, nialint b_len) {
    nialint i = 0;
    
    if (a_len == 2) {
        /* Handle as special case */
        register double a_re = *a++, a_im = *a++;
        for (i = 0; i < b_len; i += 2) {
            register double b_re = *b++, b_im = *b++;
            *dest++ = a_re*b_re - a_im*b_im;
            *dest++ = a_re*b_im + a_im*b_re;
        }
        
    } else {
        /* Multiplication of conformant arrays */
        for (i = 0; i < b_len; i += 2) {
            register double a_re = *a++, a_im = *a++;
            register double b_re = *b++, b_im = *b++;
            *dest++ = a_re*b_re - a_im*b_im;
            *dest++ = a_re*b_im + a_im*b_re;
        }
    }
    
    return;
}


/**
 * Complex sum of two arrays of numbers
 */
static void complex_sum2(double *dest, double *a, nialint a_len, double *b, nialint b_len) {
    nialint i = 0;
    
    if (a_len == 2) {
        /* Handle as special case */
        register double a_re = *a++, a_im = *a++;
        for (i = 0; i < b_len; i += 2) {
            register double b_re = *b++, b_im = *b++;
            *dest++ = a_re + b_re;
            *dest++ = a_im + b_im;;
        }
        
    } else {
        /* Sum of conformant arrays */
        for (i = 0; i < b_len; i++) {
            *dest++ = *a++ + *b++;
        }
    }
    
    return;
}


/**
 * Complex subtraction of two arrays of numbers
 */
static void complex_sub2(double *dest, double *a, nialint a_len, double *b, nialint b_len) {
    nialint i = 0;
    
    if (a_len == 2) {
        /* Handle as special case */
        register double a_re = *a++, a_im = *a++;
        for (i = 0; i < b_len; i += 2) {
            *dest++ = a_re - *b++;
            *dest++ = a_im - *b++;
        }
        
    } else if (b_len == 2) {
        /* Special case */
        register double b_re = *b++, b_im = *b++;
        for (i = 0; i < a_len; i += 2) {
            *dest++ = *a++ - b_re;
            *dest++ = *a++ - b_im;
        }
    } else {
        /* Subtraction of conformant arrays */
        for (i = 0; i < b_len; i++) {
            *dest++ = *a++ - *b++;;
        }
    }
    
    return;
}




/* ---------------- Multivalent ---------------------- */

/**
 * Complex product of the elements of an array
 */
static void complex_prod1(double *a, double *b, nialint b_len) {
    nialint i = 0;
    register double r_re = 1.0, r_im = 0.0;
    
    /* Divide all elements */
    for (i = 0; i < b_len; i += 2) {
        register double b_re = *b++, b_im = *b++;
        register double a_re = r_re, a_im = r_im;
        r_re = a_re*b_re - a_im*b_im;
        r_im = a_re*b_im + a_im*b_re;
    }
    
    a[0] = r_re;
    a[1] = r_im;
    return;
}

/**
 * Complex sequential division of the elements of an array
 */
static void complex_div1(double *a, double *b, nialint b_len) {
    nialint i = 0;
    register double r_re = *b++, r_im = *b++;
    
    /* Divide all elements */
    for (i = 0; i < b_len; i += 2) {
        register double b_re = *b++, b_im = *b++;
        register double a_re = r_re, a_im = r_im;
        register double base = b_re*b_re + b_im*b_im;
        r_re = (a_re*b_re + a_im*b_im)/base;
        r_im = (a_im*b_re - a_re*b_im)/base;
    }
    
    a[0] = r_re;
    a[1] = r_im;
    return;
}


/**
 * Complex sequential sum of the elements of an array
 */
static void complex_sum1(double *a, double *b, nialint b_len) {
    nialint i = 0;
    register double r_re = *b++, r_im = *b++;
    
    /* sum all elements */
    for (i = 2; i < b_len; i += 2) {
        r_re += *b++;
        r_im += *b++;
    }
    
    a[0] = r_re;
    a[1] = r_im;
    return;
}


/**
 * Complex sequential subtraction of the elements of an array
 */
static void complex_sub1(double *a, double *b, nialint b_len) {
    nialint i = 0;
    register double r_re = *b++, r_im = *b++;
    
    /* sum all elements */
    for (i = 2; i < b_len; i += 2) {
        r_re -= *b++;
        r_im -= *b++;
    }
    
    a[0] = r_re;
    a[1] = r_im;
    return;
}


/* ------------------- Nial Operations ----------------------------- */


/**
 * Check if array represents a valid complex array
 */
void iis_complex(void) {
    nialptr x = apop();
    
    apush((VALID_COMPLEX(x))? True_val: False_val);
    freeup(x);
    return;
}


/**
 * Product of complex numbers
 */
void ic_prod(void) {
    nialptr x = apop();
    nialptr res;
    nialint reslen = 2;

    if (tally(x) == 0) {
        apush(Null);
        freeup(x);
        return;
    }
    
    /* Multiply all the elements of an array */
    if (VALID_COMPLEX(x)) {
        res = new_create_array(realtype, 1, 0, &reslen);
        complex_prod1(pfirstreal(res), pfirstreal(x), tally(x));
        apush(res);
        freeup(x);
        return;
    }
    
    /* Multiply two or more arrays */
    if (kind(x) == atype && tally(x) > 1) {
        if (tally(x) == 2) {
            /* multiply a pair */
            nialptr a = fetch_array(x, 0), b = fetch_array(x, 1);
            if (VALID_COMPLEX(a) && VALID_COMPLEX(b)) {
                if (tally(a) == 2) {
                    res = new_create_array(realtype, valence(b), 0, shpptr(b, valence(b)));
                    complex_prod2(pfirstreal(res),
                                  pfirstreal(a), tally(a),
                                  pfirstreal(b), tally(b));
                    apush(res);
                    freeup(x);
                    return;
                } else if (equalshape(a,b) || tally(b) == 2) {
                    res = new_create_array(realtype, valence(a), 0, shpptr(a, valence(a)));
                    complex_prod2(pfirstreal(res),
                                  pfirstreal(b), tally(b),
                                  pfirstreal(a), tally(a));
                    apush(res);
                    freeup(x);
                    return;
                } else {
                    apush(makefault("?not conformant"));
                    freeup(x);
                    return;
                }
            } else {
                apush(makefault("arg_types"));
                freeup(x);
                return;
            }
        } else {
            nialint i = 0;
            apush(makefault("?not_yet"));
            freeup(x);
            return;
        }
    }

    /* No more options */
    apush(makefault("?args"));
    freeup(x)
    return;
}


/**
 * Division of complex numbers
 */
void ic_div(void) {
    nialptr x = apop();
    nialptr res;
    nialint reslen = 2;

    if (tally(x) == 0) {
        apush(Null);
        freeup(x);
        return;
    }
    
    /* Multiply all the elements of an array */
    if (VALID_COMPLEX(x)) {
        res = new_create_array(realtype, 1, 0, &reslen);
        complex_div1(pfirstreal(res), pfirstreal(x), tally(x));
        apush(res);
        freeup(x);
        return;
    }
    
    /*  two or more arrays */
    if (kind(x) == atype && tally(x) > 1) {
        if (tally(x) == 2) {
            /* multiply a pair */
            nialptr a = fetch_array(x, 0), b = fetch_array(x, 1);
            if (VALID_COMPLEX(a) && VALID_COMPLEX(b)) {
                if (tally(a) == 2) {
                    res = new_create_array(realtype, valence(b), 0, shpptr(b, valence(b)));
                    complex_div2(pfirstreal(res),
                                  pfirstreal(a), tally(a),
                                  pfirstreal(b), tally(b));
                    apush(res);
                    freeup(x);
                    return;
                } else if (equalshape(a,b) || tally(b) == 2) {
                    res = new_create_array(realtype, valence(a), 0, shpptr(a, valence(a)));
                    complex_div2(pfirstreal(res),
                                  pfirstreal(a), tally(a),
                                  pfirstreal(b), tally(b));
                    apush(res);
                    freeup(x);
                    return;
                } else {
                    apush(makefault("?not conformant"));
                    freeup(x);
                    return;
                }
            } else {
                apush(makefault("arg_types"));
                freeup(x);
                return;
            }
        } else {
            freeup(x);
            return;
        }
    }

    /* No more options */
    apush(makefault("?args"));
    freeup(x)
    return;
}


/**
 * Sum of complex numbers
 */
void ic_sum(void) {
    nialptr x = apop();
    nialptr res;
    nialint reslen = 2;

    if (tally(x) == 0) {
        apush(Null);
        freeup(x);
        return;
    }
    
    /* Add all the elements of an array */
    if (VALID_COMPLEX(x)) {
        res = new_create_array(realtype, 1, 0, &reslen);
        complex_sum1(pfirstreal(res), pfirstreal(x), tally(x));
        apush(res);
        freeup(x);
        return;
    }
    
    /* Multiply two or more arrays */
    if (kind(x) == atype && tally(x) > 1) {
        if (tally(x) == 2) {
            /* multiply a pair */
            nialptr a = fetch_array(x, 0), b = fetch_array(x, 1);
            if (VALID_COMPLEX(a) && VALID_COMPLEX(b)) {
                if (tally(a) == 2) {
                    res = new_create_array(realtype, valence(b), 0, shpptr(b, valence(b)));
                    complex_sum2(pfirstreal(res),
                                  pfirstreal(a), tally(a),
                                  pfirstreal(b), tally(b));
                    apush(res);
                    freeup(x);
                    return;
                } else if (equalshape(a,b) || tally(b) == 2) {
                    res = new_create_array(realtype, valence(a), 0, shpptr(a, valence(a)));
                    complex_sum2(pfirstreal(res),
                                  pfirstreal(b), tally(b),
                                  pfirstreal(a), tally(a));
                    apush(res);
                    freeup(x);
                    return;
                } else {
                    apush(makefault("?not conformant"));
                    freeup(x);
                    return;
                }
            } else {
                apush(makefault("arg_types"));
                freeup(x);
                return;
            }
        } else {
            freeup(x);
            return;
        }
    }

    /* No more options */
    apush(makefault("?args"));
    freeup(x)
    return;
}


/**
 * Subtraction of complex numbers
 */
void ic_sub(void) {
    nialptr x = apop();
    nialptr res;
    nialint reslen = 2;

    if (tally(x) == 0) {
        apush(Null);
        freeup(x);
        return;
    }
    
    /* Multiply all the elements of an array */
    if (VALID_COMPLEX(x)) {
        res = new_create_array(realtype, 1, 0, &reslen);
        complex_sub1(pfirstreal(res), pfirstreal(x), tally(x));
        apush(res);
        freeup(x);
        return;
    }
    
    /* Multiply two or more arrays */
    if (kind(x) == atype && tally(x) > 1) {
        if (tally(x) == 2) {
            /* subtract a pair */
            nialptr a = fetch_array(x, 0), b = fetch_array(x, 1);
            if (VALID_COMPLEX(a) && VALID_COMPLEX(b)) {
                if (tally(a) == 2) {
                    res = new_create_array(realtype, valence(b), 0, shpptr(b, valence(b)));
                    complex_sub2(pfirstreal(res),
                                  pfirstreal(a), tally(a),
                                  pfirstreal(b), tally(b));
                    apush(res);
                    freeup(x);
                    return;
                } else if (equalshape(a,b) || tally(b) == 2) {
                    res = new_create_array(realtype, valence(a), 0, shpptr(a, valence(a)));
                    complex_sub2(pfirstreal(res),
                                  pfirstreal(a), tally(a),
                                  pfirstreal(b), tally(b));
                    apush(res);
                    freeup(x);
                    return;
                } else {
                    apush(makefault("?not conformant"));
                    freeup(x);
                    return;
                }
            } else {
                apush(makefault("arg_types"));
                freeup(x);
                return;
            }
        } else {
            freeup(x);
            return;
        }
    }

    /* No more options */
    apush(makefault("?args"));
    freeup(x)
    return;
}


/**
 * Complex exponential function
 */
void ic_exp(void) {
    nialptr x = apop();
    nialptr res;
    register double *dest, *src;
    nialint i;
    
    if (VALID_COMPLEX(x)) {
        nialint x_len = tally(x);
        nialint res = new_create_array(realtype, valence(x), 0, shpptr(x, valence(x)));
        register double *dest = pfirstreal(res);
        register double *src  = pfirstreal(x);
        
        for (i = 0; i < x_len; i += 2) {
            register double m = exp(*src++);
            register double x_im = *src++;
            *dest++ = m*cos(x_im);
            *dest++ = m*sin(x_im);
        }

        apush(res);
        freeup(x);
        return;
    }
    
    apush(makefault("args"));
    freeup(x);
    return;
}


/**
 * Split a complex matrix into real and imaginary parts
 */

void isplit_complex(void) {
    nialptr x = apop();
    nialptr x_re, x_im;
    
    if (VALID_COMPLEX(x)) {
        nialint rvalence = valence(x) - 1;
        nialptr x_re = new_create_array(realtype, rvalence, 0, shpptr(x, valence(x)));
        nialptr x_im = new_create_array(realtype, rvalence, 0, shpptr(x, valence(x)));
        register double *xp = pfirstreal(x), *rep = pfirstreal(x_re), *imp = pfirstreal(x_im);
        nialint i, tx = tally(x);
        
        for (i = 0; i < tx; i += 2) {
            *rep++ = *xp++;
            *imp++ = *xp++;
        }
        
        apush(mkapair(x_re, x_im));
        freeup(x);
        return;
    }
    
    apush(makefault("?args"));
    freeup(x);
    return;
}

#endif /* NCOMPLEX */

