/*==============================================================

  MODULE   FILEIO.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This contains the system independent file i/o operations
   and support routines

================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

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

/* CLIB */
#include <ctype.h>

/* VARARGSLIB */
#include <stdarg.h>
#include <stddef.h>


/* Q'Nial header files */

#include "qniallim.h"
#include "absmach.h"
#include "basics.h"
#include "lib_main.h"
#include "if.h"
#include "fileio.h"

#include "utils.h"           /* for ngetname */

#ifdef UNIXSYS
#include "linenoise.h"
#endif


static int  putlines(FILE * fptr, nialptr y);
static int  block_array(FILE * fpr, nialptr x);
static nialptr unblock_array(FILE * fpr);
static void iwrec(int isarray);
static void ireadrec(int isarray);
static int  writerec(int isarray, nialint portno, nialint index, nialptr record);
static int  readrec(int isarray, nialint portno, nialint index);
static int  eraserecord(nialint portno, nialint recnum, int *res);
static void compressfile(nialint portno);
static void readoneline(FILE * fptr, int mode, nialint n, char openmode);
static int  writeoneline(FILE * fptr, nialptr y, char openmode);
static void put_or_append(char mode);

extern void rl_gets(char *promptstr, char *inputline);

#ifndef WINNIAL
/*extern char *sys_errlist[];*/
extern int  errno;
#endif

/* This module implements the Nial file system.

   Nial has three styles of file handling :
   - stream oriented files, 
   - direct access files built with Nial, and
   - direct access to byte chunks of host files

   The stream oriented files are treated as lines of text.

   The readfile operation reads a text string starting at the current
   file pointer position in the text and reads up to the
   end-of-line designator, which is stripped off.

   The writefile operation writes a line at the end of the file
   adding the end-of-line designator.

   The direct access files support a component based file system
   of two flavours. A file can either be one of records, where each
   record is an uninterpreted byte stream, or one of arrays, in which
   each component is an entire array encoded in a byte stream format.
   The encoding of the latter uses the internal binary format of
   numbers and text and provides an efficient means of storing data
   outside the workspace.

   The operations readrecord and writerecord support record files,
   readarray and writearray support array files.

   The file system is implemented using C structures to hold the
   data associated with each file. There is a maximum number of files
   that can be open at one time. This is set at compilation time by
   the macro MAXFILES.

   For each user created file there is :

   filename  - a Nial phrase giveing the file name (including path)
   ioport - a FILE * entry needed for the stdio routines
   filetype - the filetype used to open the file r, w, d, c
   indexioport - for direct files this gives the FILE * entry for the
   associated index file.

   The stream oriented files map onto the text files supported by
   the C stdio facility in a straightforward way. The routine
   interfaces in the *if.c files provide an abstract interface that
   can be implemented efficiently on a particular system.

   The direct access files require some explanation.

   Each direct access file is stored as a pair of host files. For example
   the Nial direct file "data" is stored as "data.rec" and "data.ndx".
   The first file is used to store the actual component information, while
   the second stores indexing and usage information. The layout of the
   index file is the following:

<---------------- filehead ----------------->|<-----------indexpairs ------->
+-----------+----------+------------+--------+---+----------+-----------+---+
| recordcnt | totallen | space_free | status |...| recstart | reclength |...|
+-----------+----------+------------+--------+---+----------+-----------+---+

   recordcnt - number of components in the file
   totallen - length of *.rec file in bytes
   space_free - amount of free space in the *.rec file
   status - indicator of file status as array, record or notdecided

   Corresponding to every component in the file, there is a pair of entries
   giving the starting position of the component and its length.

   The ordering of the components in the *.rec file may not be in order if
   the components have been written out of order, or if insertions and
   deletions of records of different sizes have occurred.

   The *.rec file is automatically compressed if the amount of wasted space
   becomes too large (if the file is more than 50% wasted space and it is
                      longer than 10000 bytes).

*/

/* local variables */
static FILE *opendeffiles[MAXFILES];  /* list of ports opened by loaddefs,
                                         these will be closed on jumps to top
                                         level */

static FILE *ioports[MAXFILES];       /* list of io ports */
static char filetypes[MAXFILES][10];  /* file types for io ports */
static FILE *indexioports[MAXFILES];  /* corresponding index io port if a
                                         direct access .rec file */


static nialint firstuserfile;  /* first io port belonging to user */

static nialint topsysfile;     /* index of top file in use by loaddefs */

static nialint nextfilenum;    /* used in opening files */
static long nextrecpos;        /* used in direct access work */

#define len_filehead 4L
#define len_indexpair 2L

/* arrays that are used to hold the filehead and an index pair.
   Assumed to be long  */
static long filehead[len_filehead],
            indexpair[len_indexpair];


/* macros for selecting from the two arrays */
#define indexaddr (char *)&indexpair[0]
#define recstart indexpair[0]
#define reclength indexpair[1]
#define reclengthaddr (nialint *)&indexpair[1]
#define hdraddr (char *) &filehead[0]
#define recordcnt filehead[0]
#define totallen filehead[1]
#define space_free filehead[2]
#define status filehead[3]

#define headersize (len_filehead*sizeof(long))
#define indexpairsize (len_indexpair*sizeof(long))


/* user file status */

#define closedport (FILE *) (-1L)


/* macros to test file status and types */
#define ispopen(i) (i>=0 && i<nextfilenum && ioports[i]!=closedport)
#define isread(i) (strchr(filetypes[i],'r') || strchr(filetypes[i],'c'))
#define iswrite(i) (strchr(filetypes[i],'w') || strchr(filetypes[i],'a') || strchr(filetypes[i],'c'))
#define isdirect(i) (strchr(filetypes[i],'d'))



#define undeclared_type 0L   /* These are to tell writerec and readrec */
#define array_type 1L        /* what type of records are contained    */
#define record_type 2L       /* in the *.rec file                      */


/* nprintf is a utility routine to support printf behaviour for writing 
   to output for debugging, messages etc. 
   The first argument holds the flags for the type of message it is.
   The second arg is a format filed as used by printf.
   The remain args are the values to be used by the format.

   The same code now appears to work for both Windows and Unix systems.
   It should be checked for other ones.
*/


int
nprintf(int flags, char *fmt,...)
{
  va_list     args;
  int         rc;
  char        output[5000];  /* It is unlikely that very long strings will be
                                 written for debugging or as messages.   */

  va_start(args, fmt);
  
  rc = vsprintf(output, fmt, args); /* returns length of output string */

  if (rc >= 4999)
    exit_cover1(" nprintf local buffer overwritten", NC_FATAL);
  /* no other flags set */
  if (flags & OF_NORMAL)
    writechars(STDOUT, output, strlen(output), false);

  /* can only have one of OF_MESSAGE or OF_DEBUG */
  if (flags & OF_MESSAGE) {
    if (messages_on && !quiet)
      writechars(STDOUT, output, strlen(output), false);
  }
#ifdef DEBUG
  else if (flags & OF_DEBUG) {
    if (debug_messages_on)
      writechars(STDOUT, output, strlen(output), false);
  }
#endif

  va_end(args);
  return (strlen(output));
}


/* routines to support the Nial direct access I/O */

/* the following define allows a universal treatment of error
   conditions occuring in attempts to read or write blocks of
   data in direct access. */

#ifdef DEBUG
#define testerr(ioop) { long flag; if ((flag=ioop)==IOERR)\
      { nprintf(OF_DEBUG,"IOERR occurred\n");return(flag); }}
#else
#define testerr(ioop) { long flag; if ((flag=ioop)==IOERR) return(flag); }
#endif

/* implements Nial primitive writerecord */

void
iwriterecord()
{
  iwrec(false);
}


/* implements Nial primitive writearray */

void
iwritearray()
{
  iwrec(true);
}

/* general routine used for both writearray and writerecord */

static void
iwrec(int isarray)
{
  nialptr     x,
              y,
              record,
              z;
  nialint     portno,
              n,
              j;
  int         flag;

  x = apop();
  if (kind(x) == inttype && tally(x) == 3) /* explode argument if all integers */
    x = explode(x, valence(x), 3, 0, 3);

  if (kind(x) == atype && tally(x) == 3) {

    /* get port number */
    y = (fetch_array(x, 0));
    if (kind(y) != inttype || !atomic(y) || intval(y) < 0) {
      buildfault("arg must be a file port");
      freeup(x);
      return;
    }
    portno = intval(y);

    /* get record number(s) in y */
    y = (fetch_array(x, 1));
    if (kind(y) != inttype) {
      buildfault("record numbers must be integers");
      freeup(x);
      return;
    }
    /* check that the port is open and of type direct */
    if (ispopen(portno)) {
      if (isdirect(portno)) {
        if (atomic(y)) {     /* a single record to write */
          /* get record no and validate it */
          n = intval(y);
          if (n < 0)
            buildfault("negative record number in a direct write");
          else {
            /* pick up the record argument and write it */
            record = (fetch_array(x, 2));
            flag = writerec(isarray, portno, n, record);
            if (flag == IOERR)
              buildfault(errmsgptr);
            else
              apush(Nullexpr);
          }
        }
        else {               /* multiple records to write */
          z = fetch_array(x, 2);
          /* check lengths of record numbers and records match */
          if (tally(y) != tally(z))
            buildfault("incorrect number of values in a direct write");
          else {
            /* check that all the record numbers are valid */
            for (j = 0; j < tally(y); j++)
              if (fetch_int(y, j) < 0) {
                buildfault("negative record number in a direct write");
                freeup(x);
                return;
              }
            for (j = 0; j < tally(y); j++) {
              /* pick up record number and corresponding record and write it */
              n = fetch_int(y, j);
              record = fetchasarray(z, j); /* may create a temporary */
              flag = writerec(isarray, portno, n, record);
              if (flag == IOERR) {
                buildfault(errmsgptr);
                freeup(x);
                return;
              }
              freeup(record); /* free if a temporary record */
            }
            apush(Nullexpr);
          }
        }
      }
      else
        buildfault("file is not opened as direct");
    }
    else
      buildfault("file is not open");
  }
  else {
    if (isarray)
      buildfault("invalid arg to writearray");
    else
      buildfault("invalid arg to writerecord");
  }
  freeup(x);
}


/* routine to implement Nial Primitive readarray */

void
ireadarray()
{
  ireadrec(true);
}



/* routine to implement Nial Primitive readrecord */

void
ireadrecord()
{
  ireadrec(false);
}


/* routine that implements the two kinds of direct reads */

static void
ireadrec(int isarray)
{
  nialptr     x,
              y,
              z = Null;
  nialint     portno,
              i,
              n = 0,
              j;
  int         flag;
  x = apop();
  /* select and validate portno and record number(s) */ 
  if (tally(x) == 2 && (kind(x) == inttype || kind(x) == atype)) {
    if (kind(x) == inttype) {
      portno = fetch_int(x, 0);
      n = fetch_int(x, 1);
      if (n < 0) {
        buildfault("negative record number in direct read");
        freeup(x);
        return;
      }
    }
    else {
      y = fetch_array(x, 0);
      z = fetch_array(x, 1);
      if (kind(y) != inttype || !atomic(y)) {
        if (isarray)
          buildfault("invalid file port in readarray");
        else
          buildfault("invalid file port in readrecord");
        freeup(x);
        return;
      }
      if (kind(z) != inttype) {
        buildfault("record numbers must be integers");
        freeup(x);
        return;
      }
      for (i = 0; i < tally(z); i++)
        if (fetch_int(z, i) < 0) {
          buildfault("negative record number in direct read");
          freeup(x);
          return;
        }
      portno = intval(y);
    }
    /* check that the port is open and is of direct type and write
       record(s) */
    if (ispopen(portno)) {
      if (isdirect(portno)) {
        if (kind(x) == inttype) { /* only one record to read */
          flag = readrec(isarray, portno, n);
          if (flag == IOERR)
            buildfault(errmsgptr);
        }
        else {               /* a list of records to read */
          if (tally(z) == 0) {  /* empty list of record numbers */
            apush(Null);
          }
          else {
            int         v = valence(z);
            y = new_create_array(atype, v, 0, shpptr(z, v));
              
            for (j = 0; j < tally(z); j++) {
              flag = readrec(isarray, portno, fetch_int(z, j));
              if (flag == IOERR) {
                buildfault(errmsgptr);
                freeup(y);
                freeup(x);
                return;
              }
              
              store_array(y, j, apop());
            }
            if (homotest(y))
              y = implode(y);
            apush(y);
          }
        }
      }
      else
        buildfault("file is not opened for direct access");
    }
    else
      buildfault("file is not open");
  }
  else {
    if (isarray)
      buildfault("invalid arg to readarray");
    else
      buildfault("invalid arg to readrecord");
  }
  freeup(x);
}

/* routine to implement the Nial primitive eraserecord */

void
ieraserecord()
{
  nialptr     x;
  nialint     portno,
              n;
  int         res = -1, /* initialized to avoid compiler warning */
              flag;

  x = apop();
  if (kind(x) == inttype && tally(x) == 2) {
    portno = fetch_int(x, 0);
    n = fetch_int(x, 1);
    if (ispopen(portno)) {
      if (isdirect(portno)) {
        flag = eraserecord(portno, n, &res);
        if (flag == IOERR)
          exit_cover1("direct access eraserecord failed", NC_WARNING);
        if (res) {
          apush(Nullexpr);
        }
        else
          apush(Eoffault);
      }
      else
        buildfault("file is not opened for direct access");
    }
    else
      buildfault("file is not open");
  }
  else
    buildfault("invalid arg to eraserecord");
  freeup(x);
}

/* routine to implement the Nial primitive filetally for a direct access file.
   The result is the number of records in the file. 
*/

 

void
ifiletally()
{
  nialptr     x;
  nialint     portno;

  x = apop();
  if (kind(x) == inttype) {
    portno = intval(x);
    if (ispopen(portno)) {
      if (isdirect(portno)) { /* read header information */
        if (readblock(indexioports[portno], (char *) hdraddr, headersize,
                    true, 0L, 0) == IOERR)
          buildfault(errmsgptr);
        else  /* result is record count */
          apush(createint((nialint) recordcnt));
      }
      else
        buildfault("filetally not available");
    }
    else
      buildfault("file not open");
  }
  else
    buildfault("arg must be file port");
  freeup(x);
}


/* routine to implement the Nial primitive open under user request. 
   It assumes that the openfile routine will either succeed returning an 
   ioport or will fail and return OPENFAILED. No knowledge of file names 
   or paths is known by this routine. It is assumed that openfile or the 
   routines it calls will check validity of the name. 
*/

void
iopen()
{
  nialptr     x,
              y,
              arg,
              name,
              n;
  nialint     portno,
              i;
  int         exists;
  char        mode[10];      /* big enough to hold a mode string */
  FILE       *fptr;

  
  arg = apop();
  if (tally(arg) != 2 || kind(arg) != atype) {
    freeup(arg);
    buildfault("open requires filename and mode");
    return;
  }
  x = fetch_array(arg, 0);
  y = fetch_array(arg, 1);
  n = ngetname(x, gcharbuf);
  if (n == 0) {
    buildfault("invalid_name");
    freeup(arg);
    return;
  }
  /* The following code prevents a '.' in file name at end of path. This
     allows the file name to be extended with .rec or .ndx. This restriction
     is only needed for the PC and related versions, but is imposed on others
     for compatiblity. Other versions may need some refining in this area. */

#ifdef BACKSLASHSEPARATOR
  i = n - 1;
  while (i >= 0 && (gcharbuf[i] != '.' && gcharbuf[i] != '\\'))
    i--;
  if (i >= 0 && gcharbuf[i] == '.' && equalsymbol(y, "D")) {
    buildfault("invalid direct file name");
    freeup(arg);
    return;
  }
#else
  i = n - 1;
  while (i >= 0 && (gcharbuf[i] != '.' && gcharbuf[i] != '/'))
    i--;
  if (i >= 0 && gcharbuf[i] == '.' && equalsymbol(y, "D")) {
    buildfault("invalid direct file name");
    freeup(arg);
    return;
  }
#endif
  name = makephrase(gcharbuf);
  /* make sure file is not already open */
  portno = 0;
  while (portno < nextfilenum && name != fetch_array(filenames, portno))
    portno++;
  /* we are assuming unique phrase representation in the != test above */

  if (portno == nextfilenum) {  /* the file is not currently open. Set
                                   mode used in calls to openfile below */
    if (equalsymbol(y, "R"))
      strcpy(mode, "r");
    else if (equalsymbol(y, "W"))
      strcpy(mode, "w");
    else if (equalsymbol(y, "A"))
      strcpy(mode, "a");
    else if (equalsymbol(y, "C"))
      strcpy(mode, "c");
    else if (equalsymbol(y, "D"))
      strcpy(mode, "d");
    /* The unix popen does not have a binary/text option (all text) */
    /* here we allow the assumed text mode switches */
    else if (equalsymbol(y, "PR"))
      strcpy(mode, "prt");
    else if (equalsymbol(y, "PW"))
      strcpy(mode, "pwt");
    /* The remaining code retians some cases that apply to a Windows version. */
    /* The code does not interfere in a Unix system so the code is left in case
       soneone builds a WINDOWS versions from the V7 source. */
    else if (equalsymbol(y, "PRT"))
      strcpy(mode, "prt");
    else if (equalsymbol(y, "PRB"))
      strcpy(mode, "prb");
    else if (equalsymbol(y, "PWT"))
      strcpy(mode, "pwt");
    else if (equalsymbol(y, "PWB"))
      strcpy(mode, "pwb");
    else
    {
      buildfault("invalid_mode");
      freeup(arg);
      return;
    }
    /* open the file with appropriate mode and choice between binary and text
     * files. */
    if (mode[0] == 'd') {
      gcharbuf[n] = '\0';
      strcat(gcharbuf, ".rec");
      fptr = openfile(gcharbuf, 'd', 'b');
    }
    else if (mode[0] == 'p') {

      /* The Unix popen does not seem to support a different binary and text
       * mode of opening the pipe, so to be safe, we will shorten the string
       * here in case someone is using the Windows/BC5.0 _popen mode */
      mode[2] = '\0';
      fptr = popen(gcharbuf, mode + 1);
      if (!fptr) {
        apush(makefault("?Failed to open pipe"));
        freeup(arg);
        freeup(name);
        return;
      }
    }
    else if (mode[0] == 'c')
      fptr = openfile(gcharbuf, 'c', 'b');
    else
      fptr = openfile(gcharbuf, mode[0], 't');

    /* all the fileopens above fall through to here. */
    if (fptr == OPENFAILED) {
      buildfault(errmsgptr);
      freeup(arg);
      freeup(name);
      return;
    }
    /* find a slot in the list of file ports */
    portno = 0;
    while (portno < nextfilenum && 
             kind(fetch_array(filenames, portno)) != faulttype)
      portno++;
    if (portno == nextfilenum) {  /* allocate a new port */
      if (nextfilenum == MAXFILES) {
        buildfault("no more file ports");
        freeup(arg);
        freeup(name);
        return;
      }
      nextfilenum++;
    }
    if (mode[0] == 'd') {    /* open the second file used for direct access */
      FILE       *fptr2;

      gcharbuf[n] = '\0';
      strcat(gcharbuf, ".ndx");
      /* determine if the file exists */
      exists = access(gcharbuf, 00) == 0;
      fptr2 = openfile(gcharbuf, 'd', 'b');
      if (fptr2 == OPENFAILED) {
        buildfault(errmsgptr);
        freeup(arg);
        freeup(name);
        closefile(fptr);     /* close the .rec file opened above */
        return;
      }
      indexioports[portno] = fptr2;
      if (!exists) {         /* the index information is not initialized */
        recordcnt = 0L;      /* set up initial info in the .ndx file */
        totallen = 0L;
        space_free = 0L;
        status = 0L;
        writeblock(fptr2, (char *) hdraddr, headersize, true, 0L, 0);
      }
    }
    else
      indexioports[portno] = closedport;  /* the second port is not used */
    /* record the file status information */
      ioports[portno] = fptr;
    store_array(filenames, portno, name);
    strcpy(filetypes[portno], mode);
    /* return the port number as the result */
    apush(createint(portno));
  }
  else                       /* the file is already open */
    buildfault("file is already open");
  freeup(arg);
}

/* routine to implement the Nial primitive close */

void
iclose()
{
  nialptr     x;
  nialint     portno;

  x = apop();
  if (kind(x) == inttype && atomic(x)) {
    portno = intval(x);
    if ((portno >= 0) && (portno < firstuserfile))
      buildfault("cannot close system file");
    else if (ispopen(portno)) {
      if (filetypes[portno][0] == 'p') {

        pclose(ioports[portno]);
      }

      /* must not close a pipe twice */
      if (filetypes[portno][0] != 'p')
        closefile(ioports[portno]);

      ioports[portno] = closedport;
      replace_array(filenames, portno, no_value); /* erase name */
      if (filetypes[portno][0] == 'd') {
        closefile(indexioports[portno]);
        indexioports[portno] = closedport;
      }
      apush(Nullexpr);
    }
    else
      buildfault("file is not open");
  }
  else
    buildfault("arg must be file port");
  freeup(x);
}

/* routine to implement Nial primitive readfile */

void
ireadfile()
{
  nialptr     x;
  nialint     portno,
              cnt = 0;
  int         expecteol;
  FILE       *filenm;

  x = apop();
  if (kind(x) == inttype) {
    if (tally(x) == 1) {
      portno = intval(x);
      expecteol = true;
    }
    else {
      if (tally(x) != 2) {
        buildfault("readfile expects 1 or 2 args only");
        freeup(x);
        return;
      }
      portno = fetch_int(x, 0);
      cnt = fetch_int(x, 1);
      expecteol = false;
    }
    if (ispopen(portno)) {
      if (isread(portno)) {
        filenm = ioports[portno];
        if (expecteol)
          readfileline(filenm, false);
        else
          readoneline(filenm, false, cnt, filetypes[portno][0]);
      }
      else
        buildfault("file is write only");
    }
    else
      buildfault("file not open");
  }
  else
    buildfault("arg must be file port");
  freeup(x);
}


/* general routine to read a string from a file which is not
   the console. This used here and in wsmanage.c.

   Reads one lines from the FILE *fptr, which is not STDIN.

   mode indicates whether to echo and log the input.
      0 - do not echo
      1 - always echo
      2 - echo if line not empty

   If the input line is longer than INBUFSIZE then the read is
   accomplished over two or more loops reading INBUFSIZE chars.

   An EOF is assumed to occur with an empty buffer. If it
   occurs as the first thing an end of file fault is returned,
   otherwise the partial line is returned and the fault is given
   on the next call to readfileline.

   IOERR is treated as a fatal error. All input received so far
   is thrown away and an error message fault returned. IOERR may
   occur on a read where some characters have been eaten.

   The return flag is true if EOF or an error is encountered.

*/

int
readfileline(FILE * fptr, int mode)
{
  int         cont,
              nlflag,
              flag;
  nialint     cnt;
  char        ch,
             *nextchunk;

  cont = true;
  ptrCbuffer = startCbuffer;
  while (cont) {             /* allow for multiple reads to get a line */
    cnt = 0;
    reservechars(INBUFSIZE + 1);
    nextchunk = ptrCbuffer;
    nlflag = false;
    clearerr(fptr);
    while (cnt < INBUFSIZE) {
      ch = getc(fptr);
      flag = ferror(fptr);
      if (flag && !feof(fptr)) {
        errmsgptr = strerror(flag);
        buildfault(errmsgptr);
        return true;
      }
      if (feof(fptr)) {      /* EOF */
        errmsgptr = "eof encountered";
        if (cnt == 0) {
          apush(Eoffault);
          return true;
        }
        nlflag = true;       /* fake a newline and return chars */
        break;
      }
      if (ch == '\r') {
	/* We do not want carriage returns in input lines */
        break;
      }
      if (ch == '\n') {
        nlflag = true;       /* an actual newline encountered */
        break;
      }
      *ptrCbuffer++ = ch;
      cnt++;
    }
    *ptrCbuffer = '\0';

    if (cnt > 0 && mode == 2)
      /* echo input during a loaddefs but do not include blank line after an
       * expression when in mode 2 */
    {
      writechars(STDOUT, (char *) nextchunk, cnt, nlflag);
    }
    if (nlflag)
      cont = false;
  }
  mknstring(startCbuffer, ptrCbuffer - startCbuffer);
  return false;
}

/* general routine to read n chars from the console or a file.
   It does not expect an eol indication.
   It echos input from STDIN to the log file if keeplog is on.

   mode indicates whether to echo and log the input.
      0 - do not echo
      1 - always echo

   openmode indicates whether the file is in "r" or "c" mode.

  IOERR is treated as a fatal error. All input received so far
  is thrown away and an error message fault returned. IOERR may
   occur on a read where some characters have been eaten.
*/

static void
readoneline(FILE * fptr, int mode, nialint n, char openmode)
{
  nialint     i,
              cnt;
  int         done;
  char        ch;

  ptrCbuffer = startCbuffer;
  reservechars(n + 1);
  /* do a null seek if a comm file */
  if (openmode == 'c')
    fseek(fptr, 0L, SEEK_CUR);
  i = 0;
  done = false;
  while (!done && i < n) {
    ch = fgetc(fptr);
    done = (feof(fptr) | ferror(fptr)); /* use of | instead of || to or words */
    if (!done) {
      *ptrCbuffer++ = ch;
      i++;
    }
  }
  cnt = i;
  if (feof(fptr) && cnt == 0) { /* return eof fault only when all chars are used */
    apush(Eoffault);
    return;
  }
  if (ferror(fptr)) {    /* left loop early because of error */
    buildfault(errmsgptr);
    return;
  }
  if (mode == 0) {     /* echo has been done by getc, but need to log it */
    if (keeplog && fptr == STDIN)
      writelog(ptrCbuffer, cnt, false);
  }
  mknstring(startCbuffer, cnt);
}

/* routine to implement Nial primitive writefile */

void
iwritefile()
{
  nialptr     arg,
              x,
              y,
              z;
  nialint     portno;
  FILE       *fptr;
  int         res,
              nlflag;

  arg = apop();
  if (tally(arg) < 2 || tally(arg) > 3 || kind(arg) != atype) {
    buildfault("invalid arg to writefile");
    freeup(arg);
    return;
  }
  x = fetch_array(arg, 0);
  y = fetch_array(arg, 1);

  /* optional third arg indicates if a newline is to be written */
  if (tally(arg) == 3) {
    z = fetch_array(arg, 2);
    if (kind(z) == inttype || kind(z) == booltype)
      nlflag = (kind(z) == inttype ? intval(z) : boolval(z));
    else {
      buildfault("invalid arg to writefile");
      freeup(arg);
      return;
    }
  }
  else
    nlflag = true;
  if (kind(x) == inttype && atomic(x)) {
    portno = intval(x);
    if (ispopen(portno)) {
      if (iswrite(portno)) {
        fptr = ioports[portno];
        if (nlflag)
          res = putlines(fptr, y);
        else
          res = writeoneline(fptr, y, filetypes[portno][0]);
        if (res) {
          apush(Nullexpr);
        }
        else
          buildfault("not char data in writefile");
      }
      else
        buildfault("file is read only");
    }
    else
      buildfault("file not open");
  }
  else
    buildfault("arg must be file port");
  freeup(arg);
}

/* routine to put an array to a file as text.
   y is assumed to be a phrase or a character array.
   If a character array of valence > 1, then the rows are written
   out as lines */

static int
putlines(FILE * fptr, nialptr y)
{
  if (istext(y)) {           /* write each of the lines of the
                                multidimensional array */
    nialint     j,
                nolines,
                linelength;
    int         v = valence(y);
    char       *tptr;

    if (tally(y) == 0)
      tptr = "";
    else
      tptr = pfirstchar(y);  /* safe: no allocations */
    nolines = 1;
    for (j = 0; j < (v - 1); j++)
      nolines *= pickshape(y, j);
    linelength = (v > 0 ? pickshape(y, v - 1) : 1);
    for (j = 0; j < nolines; j++) {
      writechars(fptr, tptr, linelength, true);
      tptr = tptr + linelength;
    }
    return (true);
  }
  else
    return (false);
}

/* routine to put one line to file without a new line.
   called by iwritefile.*/

static int
writeoneline(FILE * fptr, nialptr y, char openmode)
{
  if (istext(y)) {
    char       *tptr = pfirstchar(y); /* safe: no allocations */
    nialint     linelength = tally(y);

    if (openmode == 'c')     /* do a null seek if a comm file */
      fseek(fptr, 0L, SEEK_CUR);
    writechars(fptr, tptr, linelength, false);
    return (true);
  }
  else
    return (false);
}

/* routine to implement Nial primitive putfile */

void
iputfile()
{
  put_or_append('w');
}

/* routine to implement Nial primitive appendfile */

void
iappendfile()
{
  put_or_append('a');
}


/* routine to implement Nial primitives putfile and appendfile */


static void
put_or_append(char mode)
{
  FILE       *fptr;
  int         n;
  nialptr     arg,
              x,
              y;

  arg = apop();
  if (tally(arg) != 2 || kind(arg) != atype) {
    buildfault("invalid arg to putfile");
    freeup(arg);
    return;
  }
  x = fetch_array(arg, 0);
  y = fetch_array(arg, 1);
  n = ngetname(x, gcharbuf);
  if (n == 0) {
    buildfault("invalid_name");
    freeup(arg);
    return;
  }
  if (kind(y) == chartype) {
    apush(y);
    if (atomic(y))
      isolitary();           /* to make the char a string */
    isolitary();             /* to make a list of strings */
    y = apop();
  }
  else if (kind(y) != atype) {
    buildfault("second arg must be a list of lines");
    freeup(arg);
    return;
  }
  fptr = openfile(gcharbuf, mode, 't');
  if (fptr == OPENFAILED) {
    buildfault(errmsgptr);
    freeup(y);               /* it might be temporary or part of arg */
    freeup(arg);
    return;
  }
  {
    nialint     t = tally(y),
                i;

    for (i = 0; i < t; i++)
      if (!putlines(fptr, fetch_array(y, i))) {
        apush(makefault("?invalid line in putfile"));
        closefile(fptr);
        freeup(y);           /* it might be temporary or part of arg */
        freeup(arg);
        return;
      }
  }
  closefile(fptr);
  apush(Nullexpr);
  freeup(y);                 /* it might be temporary or part of arg */
  freeup(arg);
}

/* routine to implement getfile */

void
igetfile()
{
  FILE       *fptr;
  int         nolines,
              len;
  nialptr     arg;
  int         done;

  arg = apop();
  len = ngetname(arg, gcharbuf);
  freeup(arg);
  if (len == 0) {
    buildfault("invalid_name");
    return;
  }
  fptr = openfile(gcharbuf, 'r', 't');
  if (fptr == OPENFAILED) {
    buildfault(errmsgptr);
    return;
  }
  /* getlines and stack them */
  nolines = 0;
  done = false;
  while (!done) {
    done = readfileline(fptr, false);
    if (!done)               /* a line has been read */
      nolines++;
    else {                   /* either EOF or error */
      if (top == Eoffault)
        apop();              /* discard it */
      else if (nolines > 0) {
        nialptr     errfault = apop();

        clearstack();
        apush(errfault);
      }
    }
  }
  closefile(fptr);
  if (nolines > 0)
    mklist(nolines);         /* create list of strings */
  else
    apush(Null);
}

/* routine to create file name list and associated file information */

void
startfilesystem()
{
  nialint     portno = MAXFILES;

  filenames = new_create_array(atype, 1, 0, &portno);
  incrrefcnt(filenames);
  for (portno = 0; portno < MAXFILES; portno++) {
    store_array(filenames, (nialint) portno, no_value);
    ioports[portno] = closedport;
    strcpy(filetypes[portno], " ");
    indexioports[portno] = closedport;
  }
  /* install stdin */
  store_array(filenames, 0, makephrase("stdin"));

  ioports[0] = STDIN;
  strcpy(filetypes[0], "r");
  indexioports[0] = closedport;

  /* install stdout */
  store_array(filenames, 1, makephrase("stdout"));
  ioports[1] = STDOUT;
  strcpy(filetypes[1], "w");
  indexioports[1] = closedport;

  /* install stderr */
  store_array(filenames, 2, makephrase("stderr"));
  ioports[2] = STDERR;
  strcpy(filetypes[2], "w");
  indexioports[2] = closedport;

  nextfilenum = 3;
  firstuserfile = nextfilenum;
}

/* routine to close all user opened files.
   Done in a restart or jump to top level.
   We do not overwrite the entries in filenames with no_value,
   but instead indicate that is a closed port.
 */

void
closeuserfiles()
{
  int         portno;

  for (portno = firstuserfile; portno < nextfilenum; portno++) {
    if (ispopen(portno))
    {  closefile(ioports[portno]);
      ioports[portno] = closedport;
      /* for a direct file, close the corresponding index file */
      if (filetypes[portno][0] == 'd') {
        closefile(indexioports[portno]);
        indexioports[portno] = closedport;
      }
      ioports[portno] = closedport;
    }
  }
}


/* the following two routines are used in loaddefs to keep track
   of open system files. On a ctrl C to toplevel the routine
   cleardeffiles is called which closes them to free up file
   names. */

void
pushsysfile(FILE * pn)
{
  opendeffiles[topsysfile++] = pn;
}

void
popsysfile()
{
  topsysfile--;
}

/* The routine cleardeffiles is called after a long jump to mainloop.  It 
   is necessary since we can interrupt out of a loaddefs leaving a port open.  
   These ports are limited, so we must clean them up.
*/

void
cleardeffiles()
{
  int         portno;

  for (portno = 0; portno < topsysfile; portno++)
    closefile(opendeffiles[portno]);
  topsysfile = 0;
}


/* routine to implement the Nial primitive filestatus that displays the
   name and type of each open file port */

void
ifilestatus()
{
  nialptr     it,
              res;
  nialint     cnt,
              portno,
              j;

  cnt = 0;
  for (portno = 0; portno < nextfilenum; portno++)
    if (ispopen(portno))
      cnt++;
  res = new_create_array(atype, 1, 0, &cnt);
  j = 0;
  for (portno = 0; portno < nextfilenum; portno++)
    if (ispopen(portno)) {
      nialint     three = 3L;

      it = new_create_array(atype, 1, 0, &three);
      store_array(it, 0, createint(portno));
      store_array(it, 1, fetch_array(filenames, portno));
      store_array(it, 2, createchar(filetypes[portno][0]));
      store_array(res, j++, it);
    }
  apush(res);
}

/* implementation routines for direct access i/o */


/* routine to erase a record in direct i/o. res indicates
   whether the record was erased. It only fails if recnum is past
   the last record. The return value is IOERR if there is an i/o
   failure. */

static int
eraserecord(nialint portno, nialint recnum, int *res)
{
  long        position;
  FILE       *fpx = indexioports[portno];
  int         goback;

  testerr(readblock(fpx, (char *) hdraddr, headersize,
                    true, 0L, 0));
  if (recnum < recordcnt && recnum >=0) {  /* record at recnum exists */
    position = headersize + recnum * indexpairsize;
    /* read the index pair to get reclength */
    testerr(readblock(fpx, indexaddr, indexpairsize, true, position, 0));
    space_free = space_free + reclength;
    reclength = 0;
    /* write the index pair with reclength 0 */
    testerr(writeblock(fpx, indexaddr, indexpairsize, true, position, 0));
    if ((long) recnum == (recordcnt - 1)) { /* last record being erased */
      /* loop backward through index pairs until one with reclength > 0 */
      goback = true;
      while (goback) {
        recnum = recnum - 1;
        position = position - indexpairsize;
        testerr(readblock(fpx, indexaddr, indexpairsize, true, position, 0));
        if (reclength != 0 || recnum < 0)
          goback = false;
      }
      recordcnt = recnum + 1;
    }
    /* write header to record change to recordcnt */
    testerr(writeblock(fpx, (char *) hdraddr, headersize, true, 0L, 0));
    compressfile(portno);

    /* if an interrupt has been indicated goto top level */
#ifdef USER_BREAK_FLAG
    checksignal(NC_CS_NORMAL);
#endif
    *res = true;     /* report success */
  }
  else
    *res = false; /* report failure */
  return 0;
}

/*
readrec: read an array or a record in direct i/o
  This routine reads a direct access record using the index file to locate
  the record.  If the index is larger than the index file, the
  index file is assumed to described a file in which the the
  record file is made up of repeating blocks of the same
  structure as the index file. As long as the readblock does
  not fail the data read is assumed to be valid.
  NOTE: This trick
  cannot be played on output since it would not be possible
  to distinguish between adding a new record beyond the end of
  the file (e.g.appending at the end) and overwriting
  a block in a fixed sized unit past the first one. In the first
  case one must add a new index file entry, while in the
  second one must not.
*/

static int
readrec(int isarray, nialint portno, nialint index)
{
  long        position;
  FILE       *fpx = indexioports[portno],
             *fpr = ioports[portno];
  nialptr     result;

  /* read the header block of the index file */
  testerr(readblock(fpx, (char *) hdraddr, headersize, true, 0L, 0));

  /* This code checks to see if the status field and isarray are the same */

  switch ((int) status) {
    case undeclared_type:
        break;
    case record_type:
        if (isarray) {
          errmsgptr = " this file is for records only";
          return IOERR;
        }
        break;

    case array_type:
        if (!isarray) {
          errmsgptr = " this file is for arrays only";
          return IOERR;
        }
        break;
#ifdef DEBUG
    default:
        {
          nprintf(OF_DEBUG, "unknown status in writerec header %ld\n", status);
          nabort(NC_ABORT);
        }
#endif
  }

  if (((long) index) >= recordcnt) {
    if (recordcnt > 0 && !isarray) {  /* assume the index file is being used
                                         as a repeating record description */
      long        flag,
                  actualrecstart;
      
      /* compute index position and read the index pair */
      position = headersize + ((index % recordcnt) * indexpairsize);
      testerr(readblock(fpx, indexaddr, indexpairsize, true, position, 0));

      /* compute actual position of record and read it into result array */
      actualrecstart = (index / recordcnt) * totallen + recstart;

      result = new_create_array(chartype, 1, 0, reclengthaddr);

      flag = readblock(fpr, pfirstchar(result), reclength, true, actualrecstart, 0);

      if (flag >= 0) {

        apush(result);

        return 0;
      }
      apush(Eoffault);
      freeup(result);
      return flag;
    }
    apush(Eoffault);
    return EOF;
  }
  /* index is < recordcnt. Compute position and read indexpair */
  position = headersize + index * indexpairsize;
   
  testerr(readblock(fpx, indexaddr, indexpairsize, true, position, 0));

  if (reclength == 0L)
  { if (isarray) 
      { apush(makefault("?missing")); }
    else
      { apush(Null); }
  }
  else 
  if (isarray) 
  { /* store record position for beginning the unblock and do it */
    nextrecpos = recstart;
    result = unblock_array(fpr);

    apush(result);
  }
  else 
  { /* read the record into result array */
    result = new_create_array(chartype, 1, 0, reclengthaddr);
    testerr(readblock(fpr, pfirstchar(result),
                      reclength * sizeof(char), true, recstart, 0));
    apush(result);
  }

  return 0;
}

/* routine to write array or a given record.
   For a record it overwrites in the space if the new record size
   is <= old one, otherwise it writes it at the end.
   For an array, it writes it at the end putting out the array
   block by block and computing the space used.
*/

static int
writerec(int isarray, nialint portno, nialint index, nialptr record)
{
  FILE       *fpx = indexioports[portno],
             *fpr = ioports[portno];
  long        position,
              newrecstart,
              temp_posn,
              blocklen = 0L,
              freedspace = 0L;
    nialint   i;
    int       flag;

  /* read the header for the index file */
  testerr(readblock(fpx, (char *) hdraddr, headersize,
                    true, 0L, 0));

  /* check to see if the status field and isarray are the same */
  switch ((int) status) {
    case undeclared_type:
        if (isarray)
          status = array_type;
        else
          status = record_type;
        /* wait to write status after write of record succeeds */
        break;
    case record_type:
        if (isarray) {
          errmsgptr = " this file is for records only";
          return (IOERR);
        }
        break;

    case array_type:
        if (!isarray) {
          errmsgptr = " this file is for arrays only";
          return (IOERR);
        }
        break;
#ifdef DEBUG
    default:
        nprintf(OF_DEBUG, "wrong status in writerec header %ld\n", status);
        nabort(NC_ABORT);
#endif
  }


  if (!isarray) 
    blocklen = tally(record);/* use tally, since strlen would be confused by
                              * null characters in the block */

  /* get position in index file for this record */
  position = headersize + index * indexpairsize;

  if (index < recordcnt) {
    /* record at index exists. Get the index pair for the record */
    testerr(readblock(fpx, indexaddr, indexpairsize,
                      true, position, 0));
    if (isarray || blocklen > reclength) {  /* will add at end of record file */
      freedspace = reclength;
      newrecstart = totallen;
    }
    else
      /* will replace record in place */
    {
      freedspace = reclength - blocklen;
      newrecstart = recstart;
    }
  }
  else
    newrecstart = totallen;  
  /* write the block for this record or array */
  if (isarray) {
    /* store the start position and write the array block by block */
    nextrecpos = newrecstart;
    blocklen = block_array(fpr, record);
    if (blocklen == IOERR) {
      exit_cover1("write error in direct access", NC_WARNING);
    }
  }
  else {
    long    cnt;
    /* write the record and check correct amount written */
    cnt = writeblock(fpr, pfirstchar(record), blocklen * sizeof(char), 
                 true, newrecstart, 0);

    if (cnt != blocklen * sizeof(char)) {
      exit_cover1("write error in direct access", NC_WARNING);
    }
  }
  /* update the index record */
  if (index >= recordcnt) {  
   /* must fill the index array with pairs showing 0 length */
    recstart = totallen;
    reclength = 0L;
    for (i = recordcnt; i < index; i++) {
      temp_posn = headersize + i * indexpairsize;
      flag = writeblock(fpx, indexaddr, indexpairsize, true, temp_posn, 0);
      if (flag == IOERR) {
        exit_cover1("write error in direct access", NC_WARNING);
      }
    }
    /* append new record. No space is freed in this case */
    freedspace = 0L;
    recordcnt = (long) (index + 1);
  }
  /* rewrite the index pair */
  recstart = newrecstart;
  reclength = blocklen;
  flag = writeblock(fpx, indexaddr, indexpairsize, true, position, 0);
  if (flag == IOERR) {
    exit_cover1("write error in direct access", NC_WARNING);
  }
  /* rewrite header */
  if (totallen == newrecstart)
    totallen = totallen + blocklen;
  space_free = space_free + freedspace;
  writeblock(fpx, (char *) hdraddr, headersize, true, 0L, 0);

  /* consider compressing the file */
  compressfile(portno);
/* if a signal has been indicated goto top level */
#ifdef USER_BREAK_FLAG
  checksignal(NC_CS_OUTPUT);
#endif
  return (0);                /* to indicate a successful write */
}

/* The following code does not pad for double alignment. It
is now unnecessary because all moves from
the area is by copying characters. It does assume that all data
objects are contiguous blocks of bytes. This is a safe assumption
for all current architectures. */


static int
block_array(FILE * fpr, nialptr x)
{
  nialint     t,
              v,
              i,
              n;
  int         k;
  nialint     startrecpos = nextrecpos;

  /* write the kind field */
  k = kind(x);
  testerr(writeblock(fpr, (char *) &k, sizeof(int), true, nextrecpos, 0));
  nextrecpos += sizeof(int);

  /* write the valence */
  v = valence(x);
  testerr(writeblock(fpr, (char *) &v, sizeof(nialint), true, nextrecpos, 0));
  nextrecpos += sizeof(nialint);

    
  /* write the tally or string length of a phrase or fault */
  if (k == phrasetype || k == faulttype)
    t = strlen(pfirstchar(x));
  else
    t = tally(x);
  testerr(writeblock(fpr, (char *) &t, sizeof(nialint), true, nextrecpos, 0));
  nextrecpos += sizeof(nialint);
  if (v>0) { /* write the shape vector */
    testerr(writeblock(fpr, (char *) shpptr(x, v), v * sizeof(nialint), true, nextrecpos, 0));
    nextrecpos += v * sizeof(nialint);
  }
  if (k != atype) { /* compute size for each atomic type and write it */
    switch (k) {
      case phrasetype:
      case faulttype:
          n = tknlength(x) + 1;
          break;
      case booltype:
          n = (t / boolsPW + ((t % boolsPW) == 0 ? 0 : 1)) * sizeof(nialint);
          break;
      case chartype:
          n = t + 1;         /* leave space for terminating null */
          break;
      case inttype:
          n = t * sizeof(nialint);
          break;
      case realtype:
          n = t * sizeof(double);
          break;
#ifdef COMPLEX
      case cplxtype:
          n = t * 2 * sizeof(double);
          break;
#endif
    }
    testerr(writeblock(fpr, (char *) &n, sizeof(nialint), true, nextrecpos, 0));
    nextrecpos += sizeof(nialint);

    /* write the atomic data */
    testerr(writeblock(fpr, (char *) pfirstitem(x), n, true, nextrecpos, 0));
    nextrecpos += n;
  }
  else { /* write the array blocks for each of the items recursively */
    for (i = 0; i < t; i++) {
      int         recurflag = block_array(fpr, fetch_array(x, i));

      if (recurflag == IOERR) {
        return (recurflag);
      }
    }
  }
  return (nextrecpos - startrecpos);
}

/* unpacks an array as it reads it in from a file */

static      nialptr
unblock_array(FILE * fpr)
{
  nialint     t,
              v,
              i,
              n,
              tknsize;
  nialptr     x = Null,
              sh;
  int         k;
  /* read the kind of array */
  testerr(readblock(fpr, (char *) &k, sizeof(int), true, nextrecpos, 0));
  nextrecpos += sizeof(int);

  /* read the valence */
  testerr(readblock(fpr, (char *) &v, sizeof(nialint), true, nextrecpos, 0));
  nextrecpos += sizeof(nialint);

  /* read the tally or string length of a phrase or fault */
  testerr(readblock(fpr, (char *) &t, sizeof(nialint), true, nextrecpos, 0));
  nextrecpos += sizeof(nialint);

  /* create the spave vector and fill it if valence > 0 */

  sh = new_create_array(inttype, 1, 0, &v);
  if (v>0) {

    testerr(readblock(fpr, (char *) pfirstint(sh), v * sizeof(nialint), true, nextrecpos, 0));
    nextrecpos += v * sizeof(nialint);
  }

  /* create x as the container for the result if not a phrase or fault */
  tknsize = (k == phrasetype || k == faulttype ? t : 0);
  if (k != phrasetype && k != faulttype) {
    x = new_create_array(k, v, tknsize, pfirstint(sh)); /* invalid shape is ok */
  }
  freeup(sh);

  if (k != atype) { /* filling an atomic or homogeneous array */

    /* read the number of items */
    testerr(readblock(fpr, (char *) &n, sizeof(nialint), true, nextrecpos, 0));
    nextrecpos += sizeof(nialint);
    if (k == phrasetype) { /* create the phrase */
      testerr(readblock(fpr, (char *) gcharbuf, n, true, nextrecpos, 0));
      nextrecpos += n;
      x = makephrase(gcharbuf); /* to check for uniqueness */
    }
    else if (k == faulttype) { /* create the fault */
      testerr(readblock(fpr, (char *) gcharbuf, n, true, nextrecpos, 0));
      nextrecpos += n;
      x = makefault(gcharbuf);  /* to check for uniqueness */
    }
    else { /* fill x as a homogeneous array */
      testerr(readblock(fpr, (char *) pfirstchar(x), n, true, nextrecpos, 0));  /* fill x data */
      nextrecpos += n;

    }
  }
  else { /* fill x as atype using recursion to fill each item. */

    for (i = 0; i < t; i++) {
      int         recurflag = unblock_array(fpr);

      if (recurflag == IOERR) {
        freeup(x);
        buildfault(errmsgptr);
        return (apop());
      }
      store_array(x, i, recurflag);
    }
  }

  return (x);
}


/* routine to compress a file in direct i/o */

#define compresserr(ioop) { int flag = ioop; if (flag==IOERR) goto cleanup; }

static void
compressfile(nialint portno)
{
  long        position;
  FILE       *fpx = indexioports[portno],
             *fpr = ioports[portno],
             *newfpx = 0,
             *newfpr = 0;
  char        fname[2048],
              tpndx[2048],
              tprec[2048],
              fnndx[2048],
              fnrec[2048];
  int         i;
  float       ratio;
  long        newstart = 0L;

  compresserr(readblock(fpx, (char *) hdraddr, headersize, true, 0L, 0));
  if (totallen != 0L)
    ratio = ((float) space_free) / ((float) totallen);
  else
    ratio = 0.0;
  /* heuristic for direct write: automatic compression if the file is more
   * than 50% wasted space and it is longer than 10000 bytes */
  if (ratio > 0.50 && totallen > 10000L) {
    strcpy(fname, pfirstchar(fetch_array(filenames, portno)));
#ifdef DEBUG
    nprintf(OF_DEBUG, "direct access file %s being compressed ratio %5.2f totallen %ld\n",
            fname, ratio, totallen);
#endif

    sprintf(tpndx, "%s.tdx", fname);
    newfpx = openfile(tpndx, 'd', 'b');
    if (newfpx == OPENFAILED) {
      nprintf(OF_MESSAGE_LOG, "openfile failed in file compression on %s \n", tpndx);
      nprintf(OF_MESSAGE_LOG, " err msg %s\n", strerror(errno));
      return;
    }
    /* create temporary record filename for use in openfile */
    sprintf(tprec, "%s.trc", fname);
    newfpr = openfile(tprec, 'd', 'b');
    if (newfpr == OPENFAILED) {
      nprintf(OF_MESSAGE_LOG, "openfile failed in file compression on %s \n", tprec);
      nprintf(OF_MESSAGE_LOG, " err msg %s\n", strerror(errno));
      return;
    }
    /* store the old header to get index started correctly */
    compresserr(writeblock(newfpx, (char *) hdraddr, headersize, true, 0L, 0));
    totallen = 0L;
    /* copy the components into the new .rec file in order and update index */
    for (i = 0; i < recordcnt; i++) {
      position = headersize + i * indexpairsize;
      compresserr(readblock(fpx, indexaddr, indexpairsize, true, position, 0));
      if (reclength != 0L) {
        ptrCbuffer = startCbuffer;
        reservechars(reclength + 1);
        compresserr(readblock(fpr, ptrCbuffer, reclength, true, recstart, 0));
        compresserr(writeblock(newfpr, ptrCbuffer, reclength, true, 0L, 2));
        totallen = totallen + reclength;
      }
      /* else nothing to write to .rec file */
      /* write index block */
      recstart = newstart;
      newstart = newstart + reclength;
      compresserr(writeblock(newfpx, indexaddr, indexpairsize, true, position, 0));
    }
    /* store the finalized header for the new rec file */
    space_free = 0L;
    compresserr(writeblock(newfpx, (char *) hdraddr, headersize, true, 0L, 0));
    /* move new index file in place */
    sprintf(fnndx, "%s.ndx", fname);
    closefile(fpx);
    closefile(newfpx);
    unlink(fnndx);
    rename(tpndx, fnndx);
    indexioports[portno] = openfile(fnndx, 'd', 'b');

    /* move new rec file in place */
    sprintf(fnrec, "%s.rec", fname);
    closefile(fpr);
    closefile(newfpr);
    unlink(fnrec);
    rename(tprec, fnrec);
    ioports[portno] = openfile(fnrec, 'd', 'b');

#ifdef DEBUG
    nprintf(OF_DEBUG, "direct access file compression completed\n");
#endif
    return;
cleanup:                     /* we reach here if the compression fails */
    closefile(newfpx);
    closefile(newfpr);
    unlink(tpndx);
    unlink(tprec);
    nprintf(OF_MESSAGE_LOG, "direct access file compression failed\n");
    return;
  }
}



/* routine to implement the primitive writechars.
   writes characters to the screen with no newline */

void
iwritechars()
{
  nialptr     x;

  if (kind(top) == phrasetype)
    istring();
  x = apop();
  if (tally(x) != 0) {       /* otherwise no action to be taken */
    if (kind(x) != chartype || valence(x) > 1) {
      buildfault("not a text string");
      freeup(x);
      return;
    }
    writechars(STDOUT, pfirstchar(x), tally(x), false);
  }
  apush(Nullexpr);
  freeup(x);
}


/* output routine that displays an array value */

void
iwrite()
{
  ipicture();
  show(apop());
  apush(Nullexpr);
}

/* input routine used here and in eval.c 
   Leaves a Nial string on the stack */

void
readinput() {
  int rdlen;
  char buffer[INPUTSIZELIMIT];
 
    
  if (fgets(buffer, INPUTSIZELIMIT, STDIN) == NULL) {
    buffer[0] = '\000';
  } else {
    /* Some characters in buffer */
    rdlen = strlen(buffer);
    if (rdlen < INPUTSIZELIMIT - 1) {
      /* new line is present */
      buffer[rdlen - 1] = '\0'; /* replaces the newline */
      if (rdlen >= 2 && buffer[rdlen-2] == '\r') {
        /* Also remove CR if present */
        buffer[rdlen-2] = '\000';
      }
    }
  }

  mkstring(buffer);
  checksignal (NC_CS_INPUT);
}


#ifdef WINNIAL
/* Temp fix until we have linenoise on Windows */
static char input_line_buffer[4096];
#endif

/* A static variable for holding the line. */
static char *line_read = (char *)NULL;

/* Routine to read a string, and return a pointer to it,
   adding it to the input history if not empty. */

void
rl_gets(char *promptstr, char *inputline)
{
#ifdef UNIXSYS
    
    /* Get a line from the user. */
    line_read = linenoise(promptstr);
    
    /* If the line has any text in it, save it on the history. */
    if (line_read && *line_read)
        linenoiseHistoryAdd (line_read);
    
    if (line_read != NULL) {
        strcpy(inputline, line_read);
        linenoiseFree(line_read);
        line_read = NULL;
    }
    else
        inputline[0] = '\000';
#endif
#ifdef WINNIAL
  printf(promptstr);
  line_read = gets(input_line_buffer);
  if (line_read != NULL)
    strcpy(inputline, line_read);
  else
    inputline[0] = '\000';
    
#endif
}



/* input routine that reads input and evaluates it */

void
iread()
{
  ireadscreen();
  iexecute();
}


void
ireadscreen()
{
  nialptr     x;

  iwritechars();             /* writes the prompt */
  x = apop();                /* to remove Nullexpr */
  if (x == Nullexpr)
    readinput();
  else
    apush(x);                /* must be a fault */
}

/* routine to implement operation writescreen */

void
iwritescreen()
{
  nialptr     x;

  if (kind(top) == phrasetype)
    istring();
  x = apop();
  if (tally(x) != 0) {
    if (kind(x) != chartype) {
      freeup(x);
      buildfault("not text data");
      return;
    }
    if (valence(x) > 2) {
      freeup(x);
      buildfault("writescreen arg has valence > 2");
      return;
    }
    show(x);
  }
  else { /* use writechars to put out newline for an empty array */
    writechars(STDOUT, gcharbuf, (nialint) 0, true);
    freeup(x);
  }
  apush(Nullexpr);
}


/* routine that places character arrays of valence <= 2
   on the screen. It folds them if necessary by breaking very
   wide pictures into pages that are displayed one after another.
*/

void
show(nialptr arr)
{
  int         width,
              lpwidth,
              outlength,
              outwidth;
  int         nopages,
              lastpage,
              page,
              row;
  char       *topc,
             *nextc;

  if (valence(arr) == 2) {
    outlength = pickshape(arr, 0);
    outwidth = pickshape(arr, 1);
  }
  else
    /* list or atom */
  {
    outlength = 1;
    outwidth = tally(arr);
  }
  /* if ssize is 0, make the page width 'infinite' */
  if (ssizew == 0) {
    nopages = 1;
    lpwidth = outwidth;
    lastpage = 0;
  }
  else {
    if (outwidth == 0)
      nopages = 1;
    else
      nopages = (outwidth + ssizew - 1) / ssizew;
    lpwidth = outwidth % ssizew;  /* get width of last page */

    if (lpwidth == 0 && outwidth != 0)
      lpwidth = ssizew;
    lastpage = nopages - 1;
  }
  topc = pfirstchar(arr);    /* safe: no allocations */
  /* set for top */

  for (page = 0; page < nopages; page++) {
    width = (page == lastpage ? lpwidth : ssizew);
    nextc = topc;            /* set up base for first row of current page */
    for (row = 0; row < outlength; row++) {
      writechars(STDOUT, nextc, (nialint) width, true);

      nextc = nextc + outwidth; /* find start of next row */
    }
    if (page != lastpage)    /* empty line between pages */
      writechars(STDOUT, topc, (nialint) 0, true);
    topc = topc + width;     /* find start of next page */
  }
  freeup(arr);
}


/* routine to write to the log by opening and closing it for
  each entry. */

void
writelog(char *text, int textl, int newlineflg)
{
  FILE       *fptr;

  /* Open logfile */
  fptr = openfile(logfnm, 'a', 't');
  if (fptr == OPENFAILED) {
    exit_cover1("failed to open log file", NC_WARNING);
    keeplog = false;
  }
  /* write to logfile */
  writechars(fptr, text, textl, newlineflg);
  /* close logfile */
  closefile(fptr);
}

/* direct access routines for host files */


/* routine to implement readfield, which takes a file name,
   position and number of bytes as argument. */

void
ireadfield()
{
  nialptr     x,
              name,
              result,
              npos,
              nlen;
  int         flag;
  nialint     pos,
              length;
  char       *filenm;
  FILE       *fptr;

  x = apop();
  if (tally(x) == 3 && kind(x) == atype) {
    name = fetch_array(x, 0);
    npos = fetch_array(x, 1);
    nlen = fetch_array(x, 2);
    if (!istext(name)) {
      buildfault("invalid file name in readfield");
      freeup(x);
      return;
    }
    if (!isnonnegint(npos)) {
      buildfault("position must be a nonnegative integer");
      freeup(x);
      return;
    }
    pos = intval(npos);
    if (!isnonnegint(nlen)) {
      buildfault("field length must be a nonnegative integer");
      freeup(x);
      return;
    }
    length = intval(nlen);
    result = new_create_array(chartype, 1, 0, &length);
    filenm = pfirstchar(name);  /* safe: no allocation */
    fptr = openfile(filenm, 'r', 'b');
    if (fptr == OPENFAILED) {
      buildfault("open failed in readfield");
      freeup(x);
      return;
    }
    /* do the work of the read */
    flag = readblock(fptr, pfirstchar(result), length, true, pos, 0);
    closefile(fptr);
    if (flag == EOF) {
      apush(Eoffault);
      freeup(result);
    }
    else if (flag != length) {
      buildfault(errmsgptr);
      freeup(result);
    }
    else
      apush(result);
  }
  else
    buildfault("invalid arg to readfield");
  freeup(x);
}

/* routine to implement writefield, which takes a file name,
   position and a string as argument. */

void
iwritefield()
{
  nialptr     x,
              name,
              fieldval,
              npos;
  nialint     pos,
              length;
  int         flag;
  char       *filenm;
  FILE       *fptr;

  x = apop();
  if (tally(x) == 3 && kind(x) == atype) {
    name = fetch_array(x, 0);
    npos = fetch_array(x, 1);
    fieldval = fetch_array(x, 2);
    if (!istext(name)) {
      buildfault("invalid file name in writefield");
      freeup(x);
      return;
    }
    if (!isnonnegint(npos)) {
      buildfault("position must be a nonnegative integer");
      freeup(x);
      return;
    }
    pos = intval(npos);
    if (!istext(fieldval)) {
      buildfault("field must be a string");
      freeup(x);
      return;
    }
    length = tally(fieldval);
    filenm = pfirstchar(name);  /* safe: no allocation */
    fptr = openfile(filenm, 'd', 'b');
    if (fptr == OPENFAILED) {
      buildfault("open failed in writefield");
      freeup(x);
      return;
    }
    /* do the work of the write */
    flag = writeblock(fptr, pfirstchar(fieldval), length, true, pos, 0);
    closefile(fptr);
    if (flag != length)
      buildfault(errmsgptr);
    else
      apush(Nullexpr);
  }
  else
    buildfault("invalid arg to writefield");
  freeup(x);
}

/* routine to implement filelength which gets the byte length of a
   file given its file name. */

void
ifilelength()
{
  nialptr     name;
  nialint     filelength;
  struct stat buf;
  int         res;

  name = apop();
  if (istext(name)) {
    res = stat(pfirstchar(name), &buf);
    if (res == 0)
      filelength = buf.st_size;
    else
      filelength = (-1);
    apush(createint(filelength));
  }
  else
    buildfault("arg to filelength not a file name");

  freeup(name);
}

