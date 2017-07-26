/*==============================================================

  MODULE   DIAG.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This contains routines for debugging purposes. The code is
  omitted in a production version.

  The routines that provide Nial level deugging need to be added
  to basics.c  if they are to be accessible. Thus, a special run
  of the package builder is needed.

================================================================*/

#include "switches.h"

/* standard library header files */

/* This file contains old debugging code that executes at the Nial level.
 It is only included in a DEBUG build. It is probably not needed. */

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


/* Q'Nial header files */

#include "qniallim.h"
#include "fileio.h"
#include "absmach.h"
#include "basics.h"
#include "lib_main.h"

#ifdef DEBUG                 /* ( */


extern FILE *stream;


static char *allocstr[] = {"free", "allocated"};
static char *kindstr[] = {"illegal", "atype",
  "booltype", "inttype",
  "realtype", "cplxtype",
"chartype", "phrasetype", "faulttype"};

/* #ifdef OMITTED */               /* ( */

/* old routines used for debugging in early days. May need
fixing if tried now */

void
idumpatomtbl()
{
  nialptr     j;
  char       *str;
  int         i;

  /* Dumps atom table to the screen  */
  /* Can only be used when system is fairly safe since requires atomtable
   * entry creation and array creation */
  nprintf(OF_DEBUG_LOG, "Atom Table\n");
  for (i = 0; i < atomtblsize; i++) {
    j = atomtbl[i];
    if (j != vacant) {
      if (j == held)
        str = "Held";
      else
        str = pfirstchar(j); /* safe: no allocation */
      nprintf(OF_DEBUG_LOG, "i %d %s\n", i, str);
    }
  }
  apush(Nullexpr);
}

void
idumpheap()
{
  nialptr     x,
              next;
  int         count;

  x = apop();
  next = intval(x);
  freeup(x);
  count = 0;
  if (next == 0)
    next = membase;
  nprintf(OF_DEBUG_LOG, "Heap Dump\n");
  nprintf(OF_DEBUG_LOG, "Memory Size is %d words\n", memsize);
  do {
    wb(next);
    next = next + blksize(next);
    count++;
  }
  while (next < memsize);
  nprintf(OF_DEBUG_LOG, "total of %d chunks\n", count);
  apush(Nullexpr);
}

void
ishowfl()
{
  showfl();
  apush(Nullexpr);
}



void
iusedspace()
 /* Walks block by block through memory keeping a count of memory used. The
  * measure counted is refcnt * tally, with tally being set to equal one if
  * it is zero. iusedspace will show pictures of allocated arrays that have a
  * refcnt of 0 (arrays which have not been properly freed). */
{
  nialptr     next;
  nialint     space,
              tx;

  next = membase;
  space = 0;
  do {
    if (allocated(next)) {
      tx = tally(next);
      if (refcnt(arrayptr(next)) == 0) {
        nprintf(OF_DEBUG_LOG, "***** refcnt = 0 found in usedspace ****\n");
        nprintf(OF_DEBUG_LOG, "next is %d arrayptr next is %d\n", next, arrayptr(next));
        wb(next);
        nprintf(OF_DEBUG_LOG, "Picture:\n");
        printarray(arrayptr(next));
      }
      else
        space = space + (refcnt(next) * (tx == 0 ? 1 : tx));
      next = next + blksize(next);
    }
    else
      next = next + blksize(next);
  }
  while (next < memsize);
  apush(Nullexpr);
}

/* #endif  */           /* OMITTED ) */

void
showfl()
{
  nialptr     free;

  nprintf(OF_DEBUG_LOG, "free chain is \n");
  free = freelisthdr;
  while (free != TERMINATOR) {
    nprintf(OF_DEBUG_LOG, " %d (%d) %d %d %d %d \n", free, blksize(free),freetag(free),fwdlink(free),bcklink(free),endptr(free));
    free = fwdlink(free);
  }
  nprintf(OF_DEBUG_LOG, "\n");
}

void
chkfl()
{
  nialptr     free;

  free = fwdlink(freelisthdr);
  /* test that free list items are marked correctly */
  while (free != TERMINATOR) {
  /* nprintf(OF_DEBUG_LOG, " %d (%d) %d %d %d %d \n", free, blksize(free),freetag(free),fwdlink(free),bcklink(free),endptr(free));
   */
      
    if ((freetag(free) != FREETAG || endptr(free) != -free) && freetag(free) != LOCKEDBLOCK) {
      nprintf(OF_DEBUG_LOG, "chkfl: block %d on free chain forward link not tagged properly\n", free);
      nabort(NC_ABORT);
    }
    if (fwdlink(bcklink(free)) != free) {
      nprintf(OF_DEBUG_LOG, "chkfl: block %d on free chain backward not linked properly\n", free);
      nabort(NC_ABORT);
    }
    free = fwdlink(free);
  }
}


/* routine to walk through memory blocks to check size consistency */

char       *
memchk()
{
  nialptr     addr,
              free,
              oldaddr;
  static char buf[180];

  addr = membase;
  free = fwdlink(freelisthdr);
  if (free == TERMINATOR) {
    strcpy(buf, "memchk: the freelist is empty");
/* This is okay if memchk is called at top of expand_heap, so do
   not halt.
*/
  }
  /* test that free list items are marked correctly */
  while (free != TERMINATOR) {
      if ((freetag(free) != FREETAG || endptr(free) != -free) && freetag(free) != LOCKEDBLOCK) {
      strcpy(buf, "memchk: block %d on free chain forward link not tagged properly\n");
      wb(addr);
      goto halt;
    }
    if (fwdlink(bcklink(free)) != free) {
      strcpy(buf, "memchk: block %d on free chain backward not linked properly\n");
      wb(addr);
      goto halt;
    }
    free = fwdlink(free);
  }
  oldaddr = 0;
  while (addr >= membase && addr < memsize) {
/*
if (addr > 9900)
nprintf(OF_DEBUG_LOG,"addr %d size %d\n",addr,blksize(addr));
*/
    if (isfree(addr)) {  /* check that it is on the free list */
      free = fwdlink(freelisthdr);
      while (addr != free && free != TERMINATOR)
        free = fwdlink(free);
      if (free != addr) {
        sprintf(buf, "memchk: free block encountered not on chain %ld\n", addr);
        puts(buf);
        showfl();
        goto halt;
      }
    }
    else {
      if (endptr(addr) < 0) {
        nprintf(OF_DEBUG_LOG, "memchk: endptr marker wrong on allocated block %ld\n", addr);
        goto halt;
      }
      if (blksize(addr) < minsize) {
        sprintf(buf, "memchk: small allocated block %ld of size %ld \n", addr, blksize(addr));
        wb(addr);
        goto halt;
      }
    }
    oldaddr = addr;
    addr = addr + blksize(addr);
  }
  if (addr != memsize) {
    sprintf(buf, "memchk: addr in memchk does not match memsize");
    goto halt;
  }
  return "success";
halt:
  puts(buf);
  nabort(NC_ABORT);
  return buf;
}

/* routine to print the stack */

void
idumpstack()
{
  nialint     i;

  nprintf(OF_DEBUG_LOG, "topstack = %d\n", topstack);
  nprintf(OF_DEBUG_LOG, "stack is \n");
  for (i = 0; i <= topstack; i++)
    nprintf(OF_DEBUG_LOG, " i %d %d %d\n", i, stkarea[i], kind(stkarea[i]));
  apush(Nullexpr);
}

/* routines to be called from within a debugger to see values
   in containers */

void
drefcnt(nialptr x)
{
  nprintf(OF_DEBUG_LOG, "refcnt of %d is %d\n", x, refcnt(x));
}

void
dkind(nialptr x)
{
  nprintf(OF_DEBUG_LOG, "kind of %d is %d\n", x, kind(x));
}

void
dsize(nialptr x)
{
  nprintf(OF_DEBUG_LOG, "size of %d is %d\n", x, blksize(x));
}

void
dallocated(nialptr x)
{
  nprintf(OF_DEBUG_LOG, "allocated of %d is %d\n", x, allocated(x));
}

void
dlink(nialptr x)
{
  nprintf(OF_DEBUG_LOG, "link of %d is %d\n", x, fwdlink(x));
}


void
wb(nialptr x)
{
  nprintf(OF_DEBUG_LOG, "%s block *%d size %d ", allocstr[allocated(x)], x, blksize(x));
  if (!allocated(x))
    nprintf(OF_DEBUG_LOG, "link %d\n", fwdlink(x));
  else
    wa(arrayptr(x));
}

void
wa(nialptr x)
/* this routines prints an array, that is a block of memory representing
   an array starting with its array ref */
{
  nialint     i,
              length;

  int         cutoff = 10;

  nprintf(OF_DEBUG_LOG, "array at %d\n", x);
  nprintf(OF_DEBUG_LOG, " refcnt %d\n", refcnt(x));
  nprintf(OF_DEBUG_LOG, " numeric kind %d\n", kind(x));
  nprintf(OF_DEBUG_LOG, "kind %s\n", kindstr[(int) kind(x)]);
  if (!atomic(x)) {
    length = tally(x);
    if (length > cutoff)
      length = cutoff;
    nprintf(OF_DEBUG_LOG, ", shape");
    for (i = 0; i < valence(x); i++)
      nprintf(OF_DEBUG_LOG, " %d", pickshape(x, i));
    nprintf(OF_DEBUG_LOG, "\n");
    switch (kind(x)) {
      case atype:
          nprintf(OF_DEBUG_LOG, " value");  /* avoid fetch_array checks */
          for (i = 0; i < length; i++)
            nprintf(OF_DEBUG_LOG, " *%d", *(pfirstitem(x) + i));
          nprintf(OF_DEBUG_LOG, "\n");
          break;
      case chartype:
          nprintf(OF_DEBUG_LOG, " value ");
          for (i = 0; i < length; i++)
            nprintf(OF_DEBUG_LOG, "%c", fetch_char(x, i));
          nprintf(OF_DEBUG_LOG, "\n");
          break;
      case booltype:
          nprintf(OF_DEBUG_LOG, " value ");
          for (i = 0; i < length; i++)
            nprintf(OF_DEBUG_LOG, "%d", fetch_bool(x, i));
          nprintf(OF_DEBUG_LOG, "\n");
          break;
      case inttype:
          nprintf(OF_DEBUG_LOG, " value");
          for (i = 0; i < length; i++)
            nprintf(OF_DEBUG_LOG, " %d", fetch_int(x, i));
          nprintf(OF_DEBUG_LOG, "\n");
          break;


      case realtype:
          nprintf(OF_DEBUG_LOG, " value");
          for (i = 0; i < length; i++)
            nprintf(OF_DEBUG_LOG, " %f", fetch_real(x, i));
          nprintf(OF_DEBUG_LOG, "\n");
          break;
    }
  }
  else
    switch (kind(x)) {
      case inttype:
          nprintf(OF_DEBUG_LOG, ", value %d\n", intval(x));
          break;
      case booltype:
          nprintf(OF_DEBUG_LOG, ", value %d\n", boolval(x));
          break;
      case phrasetype:
      case faulttype:
          nprintf(OF_DEBUG_LOG, "value %s\n", pfirstchar(x));
          break;
      case chartype:
          nprintf(OF_DEBUG_LOG, ", value %c\n", charval(x));
          break;
      case realtype:
          nprintf(OF_DEBUG_LOG, ", value %f\n", fetch_real(x, 0));
          break;
    }
}


void
iwa()
{
  nialptr     x = apop();

  wa(x);
  freeup(x);
  apush(Nullexpr);
}

void
iwan()
{
  nialptr     x = apop();

  wa(intval(x));
  freeup(x);
  apush(Nullexpr);
}

void
iwbn()
{
  nialptr     x = apop();

  wb(blockptr(intval(x)));
  freeup(x);
  apush(Nullexpr);
}

/* routine that prints a nial array with no side effects */

void
printarray(nialptr x)
{
  apush(x);                  /* one is for protection */
  apush(x);
  ipicture();
  show(apop());              /* remove Nullexpr */
  apop();                    /* remove protection  for x */
}

#endif             /* ) DEBUG */
