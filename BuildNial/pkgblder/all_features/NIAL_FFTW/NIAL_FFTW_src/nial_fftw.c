 /*==============================================================

  MODULE   NIAL_FFTW.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  Interface to FFTW Fast Fourier Transform Library

================================================================*/


/* Q'Nial file that selects features */

#include "switches.h"

#ifdef NIAL_FFTW

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

#include <fftw3.h>



void ifft_forward(void) {
    nialptr x = apop();
    fftw_complex *c_in, *c_out;
    fftw_plan p;
    nialint ncv;
    nialptr res;
    
    if (kind(x) != realtype || (tally(x) % 2) != 0) {
        apush(makefault("?args"));
        freeup(x);
        return;
    }
    
    ncv = tally(x)/2;
    
    c_in  = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*ncv);
    c_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*ncv);

    p = fftw_plan_dft_1d(ncv, c_in, c_out, FFTW_FORWARD, FFTW_ESTIMATE);
    
    memcpy(c_in, pfirstreal(x), 2*ncv*sizeof(double));
    fftw_execute(p);
    
    res = new_create_array(realtype, valence(x), 0, shpptr(x, valence(x)));
    memcpy(pfirstreal(res), c_out, 2*ncv*sizeof(double));
    
    fftw_destroy_plan(p);
    fftw_free(c_in);
    fftw_free(c_out);
    
    apush(res);
    freeup(x);
    return;
}


void ifft_backward(void) {
    nialptr x = apop();
    fftw_complex *c_in, *c_out;
    fftw_plan p;
    nialint ncv;
    nialptr res;
    
    if (kind(x) != realtype || (tally(x) % 2) != 0) {
        apush(makefault("?args"));
        freeup(x);
        return;
    }
    
    ncv = tally(x)/2;
    
    c_in  = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*ncv);
    c_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*ncv);

    p = fftw_plan_dft_1d(ncv, c_in, c_out, FFTW_BACKWARD, FFTW_ESTIMATE);
    
    memcpy(c_in, pfirstreal(x), 2*ncv*sizeof(double));
    fftw_execute(p);
    
    res = new_create_array(realtype, valence(x), 0, shpptr(x, valence(x)));
    memcpy(pfirstreal(res), c_out, 2*ncv*sizeof(double));
    
    fftw_destroy_plan(p);
    fftw_free(c_in);
    fftw_free(c_out);
    
    apush(res);
    freeup(x);
    return;
}

#endif /* NIAL_FFTW */
