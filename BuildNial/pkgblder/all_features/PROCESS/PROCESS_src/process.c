/*==============================================================

  MODULE   PROCESS.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  Module to interface to process control functions

================================================================*/

/* Q'Nial file that selects features */

#include "switches.h"

#ifdef PROCESS

/* standard library header files */


/* IOLIB */
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
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
#include <pwd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

/* Q'Nial header files */

#include "absmach.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"


void
iprocess_fork(void)
{
  apush(createint(fork()));
}


void
iauthenticate(void)
{
  nialptr z = apop();
  char *username, *password, *code;
  struct passwd *pwd;
  int res;

  if (tally(z) != 2 ) {
    apush(makefault("Must supply two arguments to authenticate"));
    freeup(z);
    return;
  }
  if ((kind(z) != atype) ||
      ((kind(fetch_array(z,0)) != chartype) || 
       (kind(fetch_array(z,1)) != chartype))) {
    apush(makefault("Arguments to authenticate must be strings"));
    freeup(z);
    return;
  }

  username = pfirstchar(fetch_array(z,0));
  password = pfirstchar(fetch_array(z,1));

  pwd = getpwnam(username);

  if (!pwd) {
    apush(makefault("Username not found:  authenticate"));
    freeup(z);
    return;
  }

  {
    struct spwd *spass;

    /* many shadow systems require you to be root to get the password,
       in most cases this should already be the case when this
       function is called, except perhaps for IPC password changing
       requests */

    spass = getspnam(pwd->pw_name);
    if (spass && spass->sp_pwdp)
      pwd->pw_passwd = spass->sp_pwdp;
  }

  code = (char *) crypt(password,pwd->pw_passwd);
  res = strcmp(code,pwd->pw_passwd) == 0;
  apush(createbool(res));
}


/* A Simple interface to the one way crypt call.  This can allow us to do our
 * own secure password files 
 */
void
icrypt(void)
{
  nialptr z = apop();
  char * key, *salt;

  if (tally(z) != 2 ) {
    apush(makefault("Must supply two arguments (key and salt) to crypt"));
    freeup(z);
    return;
  }
  if ((kind(z) != atype) ||
      ((kind(fetch_array(z,0)) != chartype) ||
       (kind(fetch_array(z,1)) != chartype))) {
    apush(makefault("Arguments to crypt must be strings"));
    freeup(z);
    return;
  }
  if (tally(fetch_array(z,1)) < 2) {
    apush(makefault("Arguments 2, to crypt must be a string at least 2 chars long"));
    freeup(z);
    return;
  }

  key = pfirstchar(fetch_array(z,0));
  salt = pfirstchar(fetch_array(z,1));

  mkstring(crypt(key,salt));
  freeup(z);
}

void
isetpgid(void)
{ int res;
  nialptr z = apop();

  if ((kind(z) != inttype) || (tally(z) != 2)) {
    apush(makefault("Must supply two integers to setpgid (process ID and Process Group ID)"));
    freeup(z);
    return;
  }

  res = setpgid(fetch_int(z,0),fetch_int(z,0));
  if (res < 0) {
    buildfault("Unix setpgid failed");
  }
  else
    apush(createint(res));

  freeup(z);
}



/* routine to get Unix process id number */

void
igetpid(void)
{
  int         res = getpid();

  if (res < 0) {
    buildfault("Unix getpid failed");
  }
  else
    apush(createint(res));
}

/* routine to get Unix process id number */

void
igetgid(void)
{
  int         res = getgid();

  if (res < 0) {
    buildfault("Unix getgid failed");
  }
  else
    apush(createint(res));
}

void
ikillpg(void)
{ int res;
  nialptr z = apop();

  if ((kind(z) != inttype) || (tally(z) != 2)) {
    apush(makefault("Must supply two integers to killpg (Process Group ID and siganl ID)"));
    freeup(z);
    return;
  }

  res = killpg(fetch_int(z,0),fetch_int(z,0));
  if (res < 0) {
    buildfault("Unix killpg failed");
  }
  else
    apush(createint(res));
}

#endif /* PROCESS */
