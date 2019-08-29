/*==============================================================

  MODULE   SYSTEMOPS.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This module contains routines for Nial primitives used to set 
  system parameters and for conversions to and from raw formats.

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

/* STLIB */
#include <string.h>

/* SJLIB */
#include <setjmp.h>

/* CLIB */
#include <ctype.h>


/* Q'Nial header files */

#include "qniallim.h"
#include "absmach.h"
#include "ops.h"
#include "basics.h"
#include "lib_main.h"
#include "utils.h"           /* for ngetname */
#include "if.h"              /* for NORMALRETURN */
#include "fileio.h"          /* for closeuserfiles */
#include "eval.h"            /* for destroy_call_stack */

void
ibye()
{
    cleanup_ws();
    
    /* we may not need the next two lines */
    
    clear_abstract_machine();
    destroy_call_stack();

    /* leave nial with clean exit */
    longjmp(error_env, NC_BYE);
}


/* sets various inter globals. The arg may be a phrase or string. 
   It is mapped to upper case to make the match.
*/
void
iset(void)
{
  char       *msg;
  nialptr     name = apop();

  if (equalsymbol(name, "SKETCH")) {
    msg = (sketch ? "sketch" : "diagram");
    sketch = true;
  }
  else if (equalsymbol(name, "DIAGRAM")) {
    msg = (sketch ? "sketch" : "diagram");
    sketch = false;
  }
  else if (equalsymbol(name, "TRACE")) {
    msg = (trace ? "trace" : "notrace");
    trace = true;
  }
  else if (equalsymbol(name, "NOTRACE")) {
    msg = (trace ? "trace" : "notrace");
    trace = false;
  }
  else if (equalsymbol(name, "DECOR")) {
    msg = (decor ? "decor" : "nodecor");
    decor = true;
  }
  else if (equalsymbol(name, "NODECOR")) {
    msg = (decor ? "decor" : "nodecor");
    decor = false;
  }
  else if (equalsymbol(name, "LOG")) {
    msg = (keeplog ? "log" : "nolog");
    if (keeplog == false) {
      keeplog = true;
    }
  }
  else if (equalsymbol(name, "NOLOG")) {
    msg = (keeplog ? "log" : "nolog");
    if (keeplog) {
      keeplog = false;
    }
  }
#ifdef DEBUG
  else if (equalsymbol(name, "DEBUG")) {
    msg = (debug ? "debug" : "nodebug");
    debug = true;
  }
  else if (equalsymbol(name, "NODEBUG")) {
    msg = (debug ? "debug" : "nodebug");
    debug = false;
  }
#endif
  else {
    apush(makefault("?unknown set type"));
    freeup(name);
    return;
  }
  apush(makephrase(msg));
  freeup(name);
}

/* routine to turn on or off display of messages */

void
isetmessages(void)
{
  nialptr     x = apop();
  int         oldstatus = messages_on;

  if (atomic(x) && kind(x) == booltype)
    messages_on = boolval(x);
  else if (atomic(x) && kind(x) == inttype)
    messages_on = intval(x);
  else {
    apush(makefault("?setmessages expects a truth-value"));
    freeup(x);
    return;
  }
  apush(createbool(oldstatus));
}

/* routine to turn on or off display of debug messages */

#ifdef DEBUG
void
isetdebugmessages(void)
{
  nialptr     x = apop();
  int         oldstatus = debug_messages_on;

  if (atomic(x) && kind(x) == booltype)
    debug_messages_on = boolval(x);
  else if (atomic(x) && kind(x) == inttype)
    debug_messages_on = intval(x);
  else {
    apush(makefault("?setdebugmessages expects a truth-value"));
    freeup(x);
    return;
  }
  apush(createbool(oldstatus));
}

#endif

/* routine to set the width of output in character array displays.
   We allow for a setwidth of 0 (which means infinite width).  
   The internal code in fileio.c was previously changed to allow
   this.  This is convenienent for CGI use and other non-interactive 
   situations. 
*/

void
isetwidth(void)
{
  nialptr     z;
  nialint     ts;

  z = apop();
  if (kind(z) == inttype) {
    ts = intval(z);
    if (ts >= 0) {           
      apush(createint(ssizew));
      ssizew = ts;
    }
    else
      buildfault("width out of range");
  }
  else
    buildfault("width not an integer");
  freeup(z);
}

/* routine to set the prompt string */

void
isetprompt(void)
{
  nialptr     z;
  int         t;

  z = apop();
  if (kind(z) == phrasetype || kind(z) == chartype) { 
    /* return old prompt */
    apush(makephrase(nprompt));
    /* get new prompt */
    if (kind(z) == phrasetype) {
      t = tknlength(z);
      if (t > MAXPROMPTSIZE)
        goto spout;
      strcpy(nprompt, pfirstchar(z));
    }
    else if (kind(z) == chartype) {
      t = tally(z);
      if (t > MAXPROMPTSIZE)
        goto spout;
      strcpy(nprompt, pfirstchar(z));
    }
  }
  else
    buildfault("arg to setprompt must be string or phrase");
  freeup(z);
  return;
spout:
  freeup(apop());            /* old prompt already stacked */
  buildfault("prompt too long");
}


/* routine to set the log name */

void
isetlogname(void)
{
  nialptr     z;

  if (kind(top) == phrasetype)
    istring();
  z = apop();
  if (kind(z) != chartype || strlen(pfirstchar(z)) > MAXLOGFNMSIZE) {
    buildfault("invalid log name");
  }
  else {
    apush(makephrase(logfnm));  /* push current name */
    strcpy(logfnm, pfirstchar(z));
  }
  freeup(z);
}

/* sets user interrupts. internal flag is backward to argument */

void
isetinterrupts()
{
  nialptr     x;
  int         oldstatus;

  oldstatus = nouserinterrupts;
  x = apop();
  if (atomic(x) && kind(x) == booltype) {
    nouserinterrupts = !boolval(x);
    apush(createbool(!oldstatus));
  }
  else if (atomic(x) && kind(x) == inttype) {
    nouserinterrupts = (intval(x) != 1);
    apush(createbool(!oldstatus));
  }
  else
    buildfault("setinterrupts expects a boolean");
}


/* routines to support data conversion at Nial level */

enum {
MCPY_TORAW = 0, MCPY_FROMRAW = 1};

#if __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__

/** 
 * Support routines for toraw in the case of char arrays and little endian
 * processors
 */

void c_to_w_copy(nialint *dest, unsigned char *src, nialint len) {
  const nialint nw_size = sizeof(nialint);    /* Nial word size */
  nialint t = 0, i ,j, nw = (len+nw_size-1)/nw_size;

  /* process pure word blocks of chars */
  for (i = 0; i < nw; i++) {
    t = 0;
    for (j = 0; j < nw_size; j++)
      t = (t << 8) | *src++;
    *dest++ = t;
  }

  return;
}

#endif



/* routine to implement the Nial primitive: toraw
  
   This routine can take any non nested array as an argument, and will
   return a boolean array that represented the data in the supplied
   array.  The user MUST know the size of the units (chars = 1 byte,
   ints = 4 bytes, reals = 8 bytes, etc) in order to use the information.
  
   The result will always be a multiple of 8 * the number of bytes needed
   to store the data type.
*/



void
itoraw(void)
{
  nialptr     z = apop();
  nialptr     res;
  nialint     bits = 0;

  /* can only convert non-nested arrays */
  if (kind(z) == atype) {
    apush(makefault("?Must supply a non-nested array value to toraw"));
    freeup(z);
    return;
  }

  /* if we are given a bool array then just return it */
  if (kind(z) == booltype) {
    apush(z);
    return;
  }


  /*  for the remaining types, calculate the number of bytes */
  switch (kind(z)) {
    case inttype:
        bits = tally(z) * sizeof(nialint);
        break;
    case realtype:
        bits = tally(z) * sizeof(double);
        break;
    case chartype:
        bits = tally(z);
        break;
    case phrasetype:
    case faulttype:
        bits = strlen(pfirstchar(z));
        break;
  }

  /*  from bytes to bits */
  bits *= 8;

  /* Create the new array */
  res = new_create_array(booltype, 1, 0, &bits);

  /* This assumes the same order of bools and a bool array shows */

  /* do the data copy */
  switch (kind(z)) {
    case inttype:
        memcpy(pfirstchar(res), pfirstchar(z), bits / 8);
        break;
    case realtype:
        memcpy(pfirstchar(res), pfirstchar(z), bits / 8);
        break;

        /* Have to use strncpy here to preserve the data */
    case chartype:
    case phrasetype:
    case faulttype:
#if __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__
      c_to_w_copy(pfirstint(res), (unsigned char *)pfirstchar(z), bits/8);
#else
      memcpy(pfirstchar(res), pfirstchar(z), bits / 8);
#endif
      break;
  }


  /* push the result and leave */
  apush(res);
  freeup(z);
  return;
}



/* routine to implement the Nial primitive: fromraw
 
   This is the symmetric routine to the new toraw routine.
   it takes two arguments.  The first is used to establish the
   type of the result you wish to create.  The second argument is
   the boolean data to be used to convert to the desired type.
  
   The amount of boolean data must be a multiple of the size of
   the desired data type, otherwise a fault is generated.
  
   The result is always an array of the desired data type.
   The tally of the result is the tally of the bool array
   divided by the number of bits needed to represent the
   desired type.
 */

void
ifromraw(void)
{
  nialptr     z = apop();
  nialptr     res;
  int         slen;
  int         totype;
  int         dataerror = 0;
  nialint     numelements;
  nialptr     bools;

  if (tally(z) != 2) {
    apush(makefault("?Must supply a type and a value to fromraw"));
    freeup(z);
    return;
  }

  totype = kind(fetch_array(z, 0));

  if (totype == atype) {
    apush(makefault("?fromraw cannot convert to a nested array"));
    freeup(z);
    return;
  }

  bools = fetch_array(z, 1);

  if ((kind(bools) != booltype)) {
    apush(makefault("?Must supply a Boolean value to fromraw"));
    freeup(z);
    return;
  }

  /* no conversion needed */
  if (totype == booltype) {
    apush(bools);
    freeup(z);
    return;
  }

  /* compute number of elements by type and validate length of bools */
  slen = 0;
  switch (totype) {
    case inttype:
        numelements = tally(bools) / sizeofnialint;
        if (tally(bools) % sizeofnialint != 0)
          dataerror = 1;
        break;
    case realtype:
        numelements = tally(bools) / sizeofnialreal;
        if (tally(bools) % sizeofnialreal != 0)
          dataerror = 1;
        break;
    case phrasetype:
    case faulttype:
        slen = tally(bools) / sizeofnialchar;
        numelements = 1;
          break;
    case chartype:
        numelements = tally(bools) / sizeofnialchar;
  }

  numelements /= 8;

  if (dataerror) {
    apush(makefault("?fromraw must have correct multiple of bools for desired datatype"));
    freeup(z);
    return;
  }

  /* create the result copy */
  res = new_create_array(totype, 1, slen, &numelements);

  /* do the data copy */
  switch (totype) {
    case inttype:
        memcpy(pfirstchar(res), pfirstchar(bools), tally(bools) / 8);
        break;
    case realtype:
        memcpy(pfirstchar(res), pfirstchar(bools), tally(bools) / 8);
        break;

        /* Have to use strncpy here to preserve the data */
    case chartype:
    case phrasetype:
    case faulttype:
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      c_to_w_copy(pfirstint(res), (unsigned char *)pfirstchar(bools), tally(bools)/8);
#else
        memcpy(pfirstchar(res), pfirstchar(bools), tally(bools) / 8);
#endif
        break;
  }


  if (totype == chartype)
    *(pfirstchar(res) + numelements) = '\0';

  apush(res);
  freeup(z);
  return;

}

#ifdef COMMAND


/* routine to implement the primitive edit which edits
   a file by using an editor specified by the user environment or
   by using the default editor for a given host. */

void
iedit(void)
{
  nialptr     nm = apop();

  if (ngetname(nm, gcharbuf) == 0)
    buildfault("invalid_name");
  else {
    calleditor(gcharbuf);
    apush(Nullexpr);
  }
  freeup(nm);
}

void
ihost()
{
  nialptr     x = apop();

  if (ngetname(x, gcharbuf) == 0)
    buildfault("invalid host command");
  else {
    if (command(gcharbuf) == NORMALRETURN) {
      apush(Nullexpr);
    }
    else
      buildfault(errmsgptr);
  }
  freeup(x);
}

#endif

#ifdef TIMEHOOK

double      get_cputime();

/* routine to implement the primitive expression time
   which gives the cpu time since beginning of session in seconds */

void
itime()
{
  apush(createreal(get_cputime()));
}

/*  Timestamp is an expression that returns a character string
    which contains the time and date.
    The length and format is system dependent.
    */

void
itimestamp(void)
{
  get_tstamp(gcharbuf);
  mkstring(gcharbuf);
}

#endif


