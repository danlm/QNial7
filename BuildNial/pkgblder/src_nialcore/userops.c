/*==============================================================

MODULE   USEROPS.C

COPYRIGHT NIAL Systems Limited  1983-2016


This file can be used to add new operations for a specific need.
 It is switched in if USEROPS is defined in pkgswchs.h using
 the Pkgblder directory.

================================================================*/


/* Q'Nial file that selects features */

#include "switches.h"

#ifdef USEROPS /* ( */

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


/* Q'Nial header files */

#include "if.h"
#include "absmach.h"
#include "basics.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"


/* See the section in the Q'Nial Design document on User Defined Primitives
   for instructions  on how to  develop new primitives. */


/* place new primitive routines here */


/* code for primitive expression GetCommandLine  */

extern int  global_argc;
extern char **global_argv;

void
iGetCommandLine(void)
{ int         sz = 0;
  int         i;
  char       *tmp;

  for (i = 0; i < global_argc; i++)
    sz += strlen(global_argv[i]) + 1;

  tmp = (char *) malloc(sz + 1);
  if (!tmp)
    exit_cover1("Not enough memeory to continue", NC_FATAL);

  strcpy(tmp, "");

  for (i = 0; i < global_argc; i++) {
    char       *flg = NULL;

    if ((flg = strchr(global_argv[i], ' ')))
      strcat(tmp, "\"");
    strcat(tmp, global_argv[i]);
    if (flg)
      strcat(tmp, "\"");
    strcat(tmp, " ");
  }


  mkstring(tmp);
  free(tmp);
}

/* end of GetCommandLine */


/* code for primitive operation GetEnv ( 
 The operation "getenv" is used to access
 environment variables in the host system.
 
 getenv Envname
 
 returns the value of the named Environment variable as a string.
 
 getenv  Null
 
 returns a list of pairs, each consisting of a variable name and its string value.
 
 */

extern char **environ;

void
iGetEnv(void)
{
  nialptr     z = apop();
  char       *arg;

  if ((kind(z) != chartype) && (kind(z) != phrasetype) && (tally(z) != 0)) {
    apush(makefault("?Must supply a string or phrase to GetEnv"));
    freeup(z);
    return;
  }


  if (tally(z) == 0)
    arg = NULL;
  else
    arg = pfirstchar(z);     /* safe */

  {
    if (arg) {
      char       *buf;
      buf = getenv(arg);
      mkstring((buf ? buf : ""));
      return;
    }

    else {
      nialptr     res;
      int         pos = 0;
      nialint     numnames = 0;

      while (environ[pos]) {
        char       *str = strdup((environ[pos]));
        char       *name = strtok(str, "=");
        char       *value = strtok(NULL, "");

        numnames++;
        mkstring(name);
        if (value)
          mkstring(value);
        else
          mkstring("");
        free(str);
        pos++;
      }

      if (numnames) {
        int         i;

        res = new_create_array(atype, 1, 0, &numnames);
        for (i = (numnames - 1); i >= 0; i--) {
          nialint     two = 2;
          nialptr     pr = new_create_array(atype, 1, 0, &two);

          store_array(pr, 1, apop());
          store_array(pr, 0, apop());
          store_array(res, i, pr);
        }
        apush(res);
        freeup(z);
        return;
      }
      else {
        apush(Null);
        freeup(z);
        return;
      }
    }
  }
}

/* end of GetEnv ) */


#endif             /* ) USEROPS */

