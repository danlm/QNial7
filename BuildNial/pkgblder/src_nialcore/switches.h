/*==============================================================

  HEADER SWITCHES.H

  COPYRIGHT NIAL Systems Limited  1983-20016

  This file contains switch settings for building QNial.
  It assumes the build is for a Unix based operating system  e.g (OSX or Linux)
 and that the target architecture is an Intel like chip that supports 32 and 64 bit words.

================================================================*/


#ifndef _SWITCHES_H_
#define _SWITCHES_H_

/*
This file controls what code gets included or excluded when
the code of Q'Nial is processed.
*/


/* compiler command lines can set the following switches to remove
   some capabilities and set up variant versions.
 
      DEBUG  - removes features that interfere with debugging.
               Also includes substantial debugging checks.
      LINUX or OSX - used in the code to handle minor differences or capabilities
 
*/

#define SYSTEM
#define DIRECTIO
#define V6AT
#define COMMAND
#define TIMEHOOK
#define DIRECTIO
#define PROFILE

#include "pkgswchs.h"  /* load the defines constructed by Pkgblder */

/* switches set for LINUX or OSX builds. */

/* operating system capabilities */

#define ABORTSIGNAL
#define JOBCONTROL
#define FP_EXCEPTION_FLAG
#define USER_BREAK_FLAG

/* define these four switches below to trade speed for space */

#define FETCHARRAYMACRO
#define STOREARRAYMACRO
#define FREEUPMACRO
#define STACKMACROS

/* they should always be undefined if a DEBUG build is being made */

#ifdef DEBUG
#undef FETCHARRAYMACRO
#undef STOREARRAYMACRO
#undef FREEUPMACRO
#undef STACKMACROS
#endif

/* the following includes set up types and constants appropriate for 32 or 64 bits systems */

#include "nialtypes.h"
#include "nialconsts.h"

#endif             /* _SWITCHES_H_ */
