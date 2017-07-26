/* ==============================================================

   MODULE     WSMANAGE.C

  COPYRIGHT NIAL Systems Limited  1983-2016

   Workspace management Module.

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

/* STDLIB */
#include <stdlib.h>

/* STLIB */
#include <string.h>

/* SJLIB */
#include <setjmp.h>

/* TIMELIB */
#ifdef TIMEHOOK
#include <sys/types.h>
#ifdef UNIXSYS
#include <sys/times.h>
#endif
#ifdef WINNIAL
#include <sys/time.h>
#endif
#include <sys/param.h>
#endif


/* Q'Nial header files */

#include "wsmanage.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"
#include "if.h"

#include "lexical.h"         /* for BLANK */
#include "version.h"         /* for nialversion */
#include "utils.h"           /* for ngetname */
#include "fileio.h"          /* for nprintf */
#include "parse.h"           /* for parse */


static int  allwhitespace(char *x);


/* this set of routines manages workspace saving and loading.
   It uses the binary read and write routines of the host
   interface.
*/



#define testerr(ioop) {if(ioop==IOERR) goto fail; }

#define testrderr(ioop) { nialint flag; flag = ioop; if (flag==IOERR || flag==EOF) goto fail; }

/* routine to dump a workspace */

void
wsdump(FILE * f1)
{
  nialptr     startaddr,
              addr,
              highest,
              next;
  nialint     cnt;

  /* find the address of the highest free block */
  highest = freelisthdr,
  next = freelisthdr;
  while (next != TERMINATOR) {
    if (next > highest)
      highest = next;
    next = fwdlink(next);
  }

  /* store global giving workspace size */
  if (highest + blksize(highest) == memsize)
    wssize = highest;
  else
    wssize = memsize;        /* there is a used block at the end */

  /* write out global structure */
  testerr(writeblock(f1, (char *) &G, sizeof G, false, 0L, 0));


  /* loop to write out allocated blocks */
  addr = membase;
  startaddr = addr;
  cnt = 0;
  while (addr < memsize) {
    if (allocated(addr)) {
      cnt += blksize(addr);
      addr += blksize(addr);
    }
    else {                   /* write out the block */
#ifdef DEBUG
      if (cnt == 0) {
        nprintf(OF_DEBUG, "second empty block in a row found in wsdump\n");
        showfl();
        nabort(NC_ABORT);
      }
#endif
/* nprintf(OF_DEBUG,"writing block startaddr %d cnt %d \n",startaddr,cnt); */

      testerr(writeblock(f1, (char *) &cnt, sizeof cnt, false, 0L, 0));
      testerr(writeblock(f1, (char *) &startaddr, sizeof addr, false, 0L, 0));
      testerr(writeblock(f1, (char *) &mem[startaddr], (size_t)(bytespu * cnt), false, 0L, 0));

      /* prepare for next block after free space */

/* nprintf(OF_DEBUG,"skipping free block addr %d blksize %d\n",addr,blksize(addr)); */

      addr = addr + blksize(addr);
      startaddr = addr;
      cnt = 0;
    }
  }

  if (startaddr != memsize) 
  {/* there is a last block to write out */

    /* nprintf(OF_DEBUG,"writing last block startaddr %d cnt %d \n",startaddr,cnt); */

    testerr(writeblock(f1, (char *) &cnt, sizeof cnt, false, 0L, 0));
    testerr(writeblock(f1, (char *) &startaddr, sizeof addr, false, 0L, 0));
    testerr(writeblock(f1, (char *) &mem[startaddr], (size_t)(bytespu * cnt), false, 0L, 0));
  }

  /* write final cnt == 0 to signal end of workspace */
  cnt = 0;
  testerr(writeblock(f1, (char *) &cnt, sizeof cnt, false, 0L, 0));
  closefile(f1);
  return;

fail:  /* the testerr and testrderr macros branch here on an error */

  nprintf(OF_NORMAL_LOG, errmsgptr);
  nprintf(OF_NORMAL_LOG, "workspace failed to write correctly\n");
  closefile(f1);
  exit_cover1("Error writing specified workspace", NC_WARNING);
}


/* routine to load a workspace. Only called from main_stu.c */


void
wsload(FILE * f1)
{
  nialptr     addr,
              cnt,
              lastfree,
              nextaddr;



  /* read global structure */
  testrderr(readblock(f1, (char *) &G, sizeof G, false, 0L, 0));


  /* check that the new workspace will fit in the current memory */


  if (wssize + MINHEAPSPACE > memsize) {
    if (expansion)
      expand_heap(wssize - memsize);
    else {
        exit_cover1("Not enough memory to expand the workspace", NC_WARNING);
    }
  }

  /* set the link to the first free block */
  fwdlink(freelisthdr) = firstfree;

  /* read memory blocks */

  lastfree = freelisthdr;
  nextaddr = membase;

  testrderr(readblock(f1, (char *) &cnt, sizeof cnt, false, 0L, 0));
  testrderr(readblock(f1, (char *) &addr, sizeof addr, false, 0L, 0));
  while (cnt != 0) {

    /* nprintf(OF_DEBUG,"reading addr %d cnt %d\n",addr,cnt); */

    testrderr(readblock(f1, (char *) &mem[addr],
                        (size_t)(bytespu * cnt), false, 0L, 0));
    nextaddr = addr + cnt;

    /* get info for next block */
    testrderr(readblock(f1, (char *) &cnt, sizeof cnt, false, 0L, 0));

    if (cnt != 0) {          /* still blocks to read */
      testrderr(readblock(f1, (char *) &addr, sizeof addr, false, 0L, 0));

      /* set up free block between used blocks */
      fwdlink(lastfree) = nextaddr;
      blksize(nextaddr) = addr - nextaddr;

      /* nprintf(OF_DEBUG,"inserting freeblock %d size %d\n",nextaddr,blksize(nextaddr)); */

      bcklink(nextaddr) = lastfree;
      set_freetag(nextaddr); /* sets free tag */
      set_endinfo(nextaddr);
      lastfree = nextaddr;
    }
  }

  if (nextaddr < memsize) {  /* add in last free block */
    fwdlink(lastfree) = nextaddr;
    blksize(nextaddr) = memsize - nextaddr;

    /* nprintf(OF_DEBUG,"adding last freeblock %d size %d\n",nextaddr,blksize(nextaddr)); */

    bcklink(nextaddr) = lastfree;
    set_freetag(nextaddr);
    set_endinfo(nextaddr);
    fwdlink(nextaddr) = TERMINATOR; /* to complete the chain */
  }
  else
    fwdlink(lastfree) = TERMINATOR; /* to complete the chain */

/* reset atomtbl stuff */
atomtblsize = tally(atomtblbase);
atomtbl = pfirstitem(atomtblbase);

#ifdef DEBUG
  memchk();
#endif

  closefile(f1);
  return;

fail:
  closefile(f1);
  exit_cover1("workspace failed to read correctly", NC_WARNING);
}


#ifdef UNIXSYS
#define FILESEPARATOR '/'
#endif
#ifdef WINNIAL
#define FILESEPARATOR '\\'
#endif



/* routine to check the file extension and add it if needed.
   if the force flag is not true, then this
   will add an extension only if there
   is not one there already. 
   This permits loaddefs and -defs of files with other extensions I.e. .nfm .
*/

void
check_ext(char *str, char *ext, int force)
{
  int         addext = false,
              extpos = strlen(str) - strlen(ext);
  char       *endstr;

  if (extpos <= 0)           /* no extension possibly present */
    addext = true;
  else {
    endstr = str + extpos;
    
    if (!force) {
      int cpos = strlen(str) - 1;
      /* walk backwards until: we reach the beginning, or
         we reach a separator, or we find a "."
      */
      while ((cpos >= 0) && (str[cpos] != FILESEPARATOR)) {
        if (str[cpos--] == '.') return;
      }
    }

    if (STRCASECMP(endstr, ext) != 0)
      addext = true;
  }

  if (addext)
    strcat(str, ext);
}


/* routine that implements the primitive save. */

void
isave(void)
{
  nialptr     x = apop();
  int         n = ngetname(x, gcharbuf);

  freeup(x);                 /* not needed */
  if (n == 0)
    buildfault("invalid file name");
  else {
    FILE       *f1;

    check_ext(gcharbuf, ".nws",NOFORCE_EXTENSION);
    f1 = openfile(gcharbuf, 'w', 'b');
    if (f1 == OPENFAILED)
      exit_cover1("cannot open nws file",NC_FATAL);
    wsfileport = f1;
    /* jump to top level loop */
    longjmp(error_env, NC_WS_SAVE);
  }
}


/* routine to implement the primitive load. */

void
iload(void)
{
  nialptr     x = apop();
  int         n = ngetname(x, gcharbuf);
  freeup(x);
  if (n == 0)
    buildfault("invalid file name");
  else {
    FILE       *f1;

    check_ext(gcharbuf, ".nws",NOFORCE_EXTENSION);
    f1 = openfile(gcharbuf, 'r', 'b');
    if (f1 == OPENFAILED) {
      buildfault("cannot open nws file");
      return;
    }
    wsfileport = f1;
    /* jump to top level loop */
    longjmp(error_env, NC_WS_LOAD);
  }

#ifdef DEBUG
  if (debug) {
    memchk();
    nprintf(OF_DEBUG, "after iload memchk\n");
  }
#endif
}

/* routine to implement the primitive loaddefs, which
   executes a script of definitions and actions. */


void
iloaddefs(void)
{
  nialptr     nm,
              x = apop();
  int         mode;

  /* get the file name as a Nial array */
  if (atomic(x) || kind(x) == chartype)
    nm = x;
  else if (kind(x) == atype)
    nm = fetch_array(x, 0);
  else {
    buildfault("invalid file name");
    freeup(x);
    return;
  }

  mode = 0;  /* default to silent mode */
  if (kind(x) == atype && tally(x) == 2) {
    /* argument has a mode filed, select it */
    nialptr     it = fetch_array(x, 1);

    if (kind(it) == inttype)
      mode = intval(it);
    if (kind(it) == booltype)
      mode = boolval(it);
  }

  /* try to put filename into gcharbuf */
  if (!ngetname(nm, gcharbuf)) {
    buildfault("invalid file name");
    freeup(x);
  }

  else 
  { /* check the extension as .ndf */
    check_ext(gcharbuf, ".ndf",NOFORCE_EXTENSION);
    freeup(x);      /* do freeup here so file name doesn't show in iusedspace */
    /* load the definition file */
    if (loaddefs(true, gcharbuf, mode)) {
      apush(Nullexpr);
    }
    else
      buildfault(errmsgptr); /* this is safe since call is from iloaddefs */
  }

#ifdef DEBUG
  memchk();
#endif
}


/* routine that does the loaddefs work. It
   loads definitions and actions from file fname, echoing the text
   and results if mode is true.   In order for the log to
   look reasonable, a space is echoed after the result value has
   been printed and the empty line indicating the end of a unit
   to be evaled is not echoed.
*/

/* defstbl.h holds the contents of the defs.ndf generated in pkgblder.
   It is used instead of the file when fromfile is false. */

#include "defstbl.h"

int
loaddefs(int fromfile, char *fname, int mode)
{
  nialptr     ts;
  int         repeatloop,
              keepreading,
              nolines,
              inremark,
              linecnt;
  FILE       *f1 = NULL;     /* initialized to avoid complaint */
  int         errorsfound;
    
  if (fromfile) {
    f1 = openfile(fname, 'r', 't');
    if (f1 == OPENFAILED)
      return (false);

    pushsysfile(f1);
  }
  /* a loaddefs always affects the global environment. We reset current_env to
     relect this.  The code to restore the environment is below. This must be
     saved on the stack, otherwise it can get thrown away since it may only
     be owned by a transient definition value. The following example failed
     before I protected this on the stack: retry is { host 'vi bug.ndf';
     loaddefs"bug l } where this definition was in the file bug.ndf. */

  apush(current_env);
  current_env = Null;

  ts = topstack;             /* to monitor stack growth on each action */
  errorsfound = 0;           /* reset parse error counter */
  repeatloop = true;
  linecnt = 0;

  /* loop to pick up groups of lines */
  while (repeatloop) {      
    /* continue as long as their are line groups */

    /* test on each circuit if an interrupt signal has been posted */
#ifdef USER_BREAK_FLAG
    if (fromfile)
      checksignal(NC_CS_NORMAL);
#endif

    inremark = false;
    nolines = 0;
    keepreading = true;

    /* loop to pick up lines until a whitespace line occurs */
    while (keepreading) {
      if (fromfile) {

        /* reading a line from the file */
          readfileline(f1, (mode ? 2 : 0)); /* mode==2 only in a loaddefs */
 
        /* readfileline places result on the stack */
        if (top == Eoffault) {
          apop();            /* to remove the end of file marker */
          repeatloop = false;
          break;             /* to end read loop */
        }
      }

      else {
        /* select a line from array defsndf loadded from defstbl.h */
        char       *line;

        line = defsndf[linecnt++];
        if (linecnt == NOLINES) {
          repeatloop = false;
          keepreading = false;  /* to end read loop */
        }
        mkstring(line);      /* convert the line to a Nial string and push it */
      }

      if (nolines == 0) {    /* check first line of group for a remark */
        char        firstchar;
        int         i = 0;

        /* loop to skip blanks */
        while (i < tally(top) && fetch_char(top, i) <= BLANK)
          i++;

        /* note whether first char is "#" */
        firstchar = fetch_char(top, i);
        if (tally(top))
          inremark = firstchar == HASHSYMBOL;
        else
          inremark = false;
      }

      /* if the line is all while space then we are at the end of a group */
      if (top == Null || allwhitespace(pfirstchar(top))) {
        keepreading = false;
        freeup(apop());      /* to get rid of the empty line */
      }
      else                   /* count the line on the stack */
        nolines++;
    }

    /* we have a group of lines to process */
    if (nolines > 0) {
      mklist(nolines);       /* create a list of lines  and link them*/
      ilink();
      if (inremark) {
        freeup(apop()); /* remarks are ignored */
      }                      
      else 
      {                 
        /* carry out the actions of the main loop */
        iscan();
        parse(true);
        /* check whether parse produced an error */
        if (kind(top) == faulttype) {
          if (top != Nullexpr) {
            errorsfound++;
            if (mode == 0) { /* show error message */
              apush(top);
              ipicture();
              show(apop());
            }
          }
        }

        /* evaluate the parse tree, if it is a fault, it is the value returned */
        ieval();

#ifdef DEBUG
        memchk();
#endif

        if (mode) {  /* show the result */
          if (top != Nullexpr) {
            ipicture();
            show(apop());
          }
          else
            apop();          /* the Nullexpr */
        }
        else
          freeup(apop());    /* free because it might not be Nullexpr */
      }

      if (mode) {            /* now display empty line */
        writechars(STDOUT, "", (nialint) 0, true);
        if (keeplog && f1 == STDIN)
          writelog("", 0, true);
      }
    }
    /* check that the stack hasn't grown */
    if (ts != topstack) {
      while (ts != topstack)
        freeup(apop());
      exit_cover1("Stack has grown during loaddefs", NC_FATAL);
    }
  } 

  /* done reading groups of lines */
  if (fromfile) {
    closefile(f1);
    popsysfile();
  }

  /* restore the current_env */
  current_env = apop();

  if (errorsfound > 0)
    nprintf(OF_NORMAL_LOG, "errors found: %d\n", errorsfound);
  return (true);
}

/* support routine to check if a line is all white space */

static int
allwhitespace(char *x)
{
  nialint     i = 0,
              n = strlen(x);

  while (i < n && ((*(x + i)) <= BLANK))
    i++;
  return (i == n);
}


