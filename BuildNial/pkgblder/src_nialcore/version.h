/*==============================================================

  VERSION.H:  

  COPYRIGHT NIAL Systems Limited  1983-2005

  This defines macros to set up the banner line for various versions.

================================================================*/

#ifndef _VERSION_H_
#define	_VERSION_H_

/* The banner information is constructed from:
   special
   nialversion
   machine
   wordsize
   opsys
   debugstatus
   date
*/

/*
   The internal variable systemname is used to set the Nial
   expression System that is available for programs to test which
   version is being used.  Only one of these cases is chosen.
*/

#ifndef __DATE__

/* compiler does not provide date functionality, set date directly
   below and remove the words (Possibly incorrect)*/

#define __DATE__ "Feb 07, 2007 (Possibly incorrect)"

#endif

/* Version 6.3 is the public domain version released as open source */

#define nialversion	" 7.0"


/*
   The internal variable systemname is used to set the Nial
   expression System that is available for programs to test which
   version is being used.  Only one of these cases is chosen.
*/



#ifdef LINUX
#define opsys           " Linux"
#endif



#ifdef OSX
#define opsys		" Mac OSX"
#endif

#ifdef WINNIAL
#define opsys    "Windows"
#endif


/* inducate word size */

#ifdef INTS32
#define wordsize " 32bit"
#elif INTS64
#define wordsize " 64bit"
#else
#error set wordsize for this configuration
#endif

#ifdef DEBUG
#define debugstatus	" DEBUG"
#else
#define debugstatus	""
#endif


#endif
