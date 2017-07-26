/* ==============================================================

   MODULE     PICTURE.C

  COPYRIGHT NIAL Systems Limited  1983-2016


  This module is responsible for the display of Nial arrays in
  interactive mode.

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

/* STLIB */
#include <string.h>

/* SIGLIB */
#include <signal.h>

/* SJLIB */
#include <setjmp.h>

/* MATHLIB */
#include <math.h>


/* Q'Nial header files */

#include "picture.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"

#include "ops.h"             /* for pair */
#include "trs.h"             /* for int_each */
#include "lexical.h"         /* for BLANK */
#include "fileio.h"          /* for nprintf */
#include "if.h"              /* for checksignal */


static int  realtochar(double x, char *buf);
static int  disp(nialptr x, int displaymode);
static nialptr paste(nialptr x, int vpad, int hpad, int vlines, int hlines, nialptr vjust, nialptr hjust, int emptysw);
static void pospaste(nialptr x, int vpad, int hpad, int vlines, int hlines);

/* defines known only to picture.c */

#define vtop    0
#define vcenter 1
#define vbottom 2

#define hleft   0
#define hcenter 1
#define hright  2

#define avtop    Zero
#define avcenter One
#define avbottom Two

#define ahleft   Zero
#define ahcenter One
#define ahright  Two

#define DEFAULTREALFORMAT "%g"
#define FULLREALFORMAT "%.17g"

/* NDINT is used in reserving space to convert integers to strings. */
 
#ifdef INTS32
#define NDINT 12

#elif INTS64
#define NDINT 21

#else
#error missing NDINT specification in picture.c
#endif

/* NDREAL is used in reserving space to convert doubles to strings.
 It i much larger than necessary since the actual format used to
 display adouble can be affected by uing "setformat" The former setting of 25
 caused a problem when setformat '%25.17g' was used.
 */

#define NDREAL 100


/* The code for picture was taken from the release 2 code.
   It was written by Jean Michel, drawing from the original by
   Lynn Sutherland. It was been modified to fit into release 1
   by Mike Jenkins.
*/

static int  luc,
            ruc,
            llc,
            rlc,
            ut,
            lt,
            rt,
            gt,
            cro,
            hor,
            ver;

void
initboxchars(int boxchars_on)
{
  if (boxchars_on) {
    luc = ibm_luc;
    ruc = ibm_ruc;
    llc = ibm_llc;
    rlc = ibm_rlc;
    ut = ibm_ut;
    lt = ibm_lt;
    rt = ibm_rt;
    gt = ibm_gt;
    cro = ibm_cro;
    hor = ibm_hor;
    ver = ibm_ver;

  }
  else {
    luc = def_luc;
    ruc = def_ruc;
    llc = def_llc;
    rlc = def_rlc;
    ut = def_ut;
    lt = def_lt;
    rt = def_rt;
    gt = def_gt;
    cro = def_cro;
    hor = def_hor;
    ver = def_ver;
  }
}


/* It uses the standard formatting provided by C. 
   It defaults to use DEFAULTREALFORMAT.
   The code assumes that "stdformat" contains the format string needed 
   for current form. It is set by default and modified by setformat. 
   The core routine produces a result with a decimal point or an exponential part. 
   This is modified afterwards to get the expected Nial format.
*/

#define FORMAT_LEN      20

char stdformat[FORMAT_LEN];
static char svdformat[FORMAT_LEN];


/* routine used in coreif.c to pick up the format routine */

char       *
getformat(void)
{
  return (stdformat);
}

/* routine used in coreif.c to set the format string */

void
setformat(char *newformat)
{
  strncpy(stdformat, newformat, FORMAT_LEN - 1);
  stdformat[FORMAT_LEN - 1] = '\0';
}

/* Nial primitive to set the real number format */

void
isetformat()
{
  nialptr     z;

  z = apop();
  if (!istext(z))
    buildfault("invalid format");
  else {
    mkstring(stdformat);     /* push old format string as result */
    if (tally(z) == 0)       /* empty arg requests default be used */
      strcpy(stdformat, DEFAULTREALFORMAT);
    else
      strcpy(stdformat, pfirstchar(z));
  }
  freeup(z);
}

/* routine to convert a real number to its character string representation */

static int
realtochar(double x, char *buf)
{
  int         i,
              n,
              found = false;

  sprintf(buf, stdformat, x);/* format given the C format */
  n = strlen(buf);
    /* correct for funny in sprintf. Sometimes it has - sign */
  if (x == 0.)              
    if (buf[0] == '-')
      strncpy(buf, buf + 1, n);

  /* search for a dot or e. If not there add a decimal point */
  n = strlen(buf);

  i = 0;
  while (i < n && !found) {
    found = buf[i] == '.' || buf[i] == 'e';
    i++;
  }

  if (!found)                /* add the dot */
    strcat(buf, ".");
  return (strlen(buf));
}


/* isketch implements the primitive operation sketch which produces
   a character table that pictures the array with some detail omitted.
   For simple arrays, the lines are not drawn, hence simple solitaries
   appear the same as the corresponding atoms. Also empty arrays
   are denoted by an empty table.

   The Cbuffer is used to hold the result for
   atoms and lists of atoms. This avoids the use of paste
   for simple arrays that are of valence < 2 .

   For higher valent arrays the routine is called recursively on each of
   the items and paste is called to build the rectangular result.
   */

void
isketch()
{
  nialptr     z = Null,      /* to avoid gcc warning */
              x = apop(),
              it,
              hjust;  /* array for horizontal adjustments */
  int         hpad, /* flag to indicate if horizontal pad is needed */
              vx = valence(x);
  nialint     i,
              sz,
              sh[2],
              tx = tally(x);
  char        *ptrc;

  ptrCbuffer = startCbuffer; /* used by disp to gather the string */

  if (atomic(x) || (vx == 1 && (simple(x) && tally(x) > 0))) {  
    /* call the routine disp() for atoms and non-empty simple lists */
    sz = disp(x, false);     /* disp() stores result in Cbuffer and frees x */

    /* create the result array */
    sh[0] = 1;
    sh[1] = sz;
    z = new_create_array(chartype, 2, 0, sh);

   /* copy the string item by item in case it has embedded nulls */
    ptrc = pfirstchar(z);  /* safe, no allocation */
    ptrCbuffer = startCbuffer;
    for (i = 0; i < tally(z); i++)  
      *ptrc++ = *ptrCbuffer++;
    *ptrc = '\0';
  }

  else 
  /* handle the empty case */
  if (0 == tally(x)) {
    /* result is a 1 by 0 character table */
    sh[0] = 1;
    sh[1] = 0;
    z = new_create_array(atype, 2, 0, sh);
    freeup(x);
  }

  else 
  /* handle the non empty cases */
    switch (kind(x)) {

      case chartype:
          if (vx == 2 && !decor) { /* picture is the argument */
            z = x;  /* do not free x since it is in use as z */
            break;
          }
          else
            goto joinsimple;

      case booltype:
          if (vx == 2 && !decor) { /* use l's and o's for bools */

            /* create result of same shape as argument and fill it */
            z = new_create_array(chartype, vx, 1, shpptr(x, vx));
            for (i = 0; i < tally(x); i++)
              store_char(z, i, (fetch_bool(x, i) ? 'l' : 'o'));
            freeup(x);
            break;
          }
          /* else falls through */
      case inttype:
      case realtype:

 joinsimple:   /* use isketch recursively on items and paste */
          {
            /* set flag to indicate if horizontal pad is needed */
            hpad = (kind(x) != chartype && kind(x) != booltype);

            int_each(isketch, x); /* note that x may be freed here */
            z = paste(apop(), 0, hpad,
                      false, false, avtop, ahright, false);
          }
          break;

      case atype:  /* set up adjustment by item */
          {
            /* create array of adjustments and fill it */
            hjust = new_create_array(inttype, vx, 0, shpptr(x, vx));
            for (i = 0; i < tx; i++) {
              it = fetch_array(x, i);
              store_int(hjust, i,
                        (atomic(it) && numeric(kind(it)) ? hright : hleft));
            }

            /* set the horizontal pad */
            hpad = simple(x) && tally(x) > 0;

            /* sketch each item and paste */
            int_each(isketch, x); /* note that x may be freed here */
            z = paste(apop(), 0, hpad, !hpad, !hpad, avtop, hjust, false);
            break;
          }
    }
  apush(z); /* push the result */
}

/* routine to implement the Nial primitive picture */

void
ipicture()
{
  if (sketch)
    isketch();
  else
    idiagram();
}

/* routine to compute the full diagram for an array. It calls
   diagram recusively on the items and uses paste to set up the
   rectangular arrangement. 
   */

void
idiagram()
{
  if (atomic(top))           /* same as sketch */
    isketch();

  else { /* nonatomic cases */
    nialptr     z,
                x = apop();
    int         vx = valence(x);

    if (tally(x) == 0) { /* empty case */

      /* create an array of blanks of the same shape and paste them together */
      apush(x);
      ishape();
      reshape(apop(), Blank);
      z = paste(apop(), 0, 0, true, true, avtop, ahleft, true);
    }

    else { /* nonempty cases handled by using diagram on the items
              and pasting them together. */
      int         kx = kind(x);
      nialptr     hjust = new_create_array(inttype, vx, 0, shpptr(x, vx));
      nialint     i,
                  tx = tally(x);
      int         itjust;

      /* compute horizontal adjustment for each item */
      for (i = 0; i < tx; i++) {
        if (kx == atype) {
          nialptr     it = fetch_array(x, i);

          itjust = (atomic(it) && numeric(kind(it)) ? hright : hleft);
        }
        else
          itjust = (numeric(kx) ? hright : hleft);
        store_int(hjust, i, itjust);
      }
      int_each(idiagram, x);
      z = paste(apop(), 0, 0, true, true, avtop, hjust, true);
    }
    apush(z); /* push the result */
  }
}


/* idisplay implements the operation display which converts a Nial
   data structure into a string, which when executed computes
   the same array. */

void
idisplay()
{
  nialptr     x,
              z;
  nialint     sz;
  int         svdecor;

  x = apop();
  /* save settings for rformat and decor */
  strcpy(svdformat, stdformat);
  svdecor = decor;

  /* set stdformat to display full real numbers and turn off decor */
  strcpy(stdformat, FULLREALFORMAT);
  decor = false;

  ptrCbuffer = startCbuffer; /* used by disp to gather the string */
  sz = disp(x, true);        /* disp() frees x */

  /* create the result array */
  z = new_create_array(chartype, 1, 0, &sz);
  {
    nialint     i;
    char       *ptrc = pfirstchar(z); /* safe, no allocation */

    ptrCbuffer = startCbuffer;
    /* copy the string item by item in case it has embedded nulls */
    for (i = 0; i < tally(z); i++)  
      *ptrc++ = *ptrCbuffer++;
    *ptrc = '\0';
  }
  
 /* restore global real format settings and decor setting */
  strcpy(stdformat, svdformat);
  decor = svdecor;

  apush(z); /* push the result */
}

#define ENDCHARS " ()[]{}#,;"


/* disp is used by both isketch and idisplay and calls itself
   recursively.
   In isketch it is called with displaymode false.
   The disp routine must explicitly test for decor mode
   for those cases where it could be called from sketch with decor
   mode on. In most cases disp does just the same work as required
   when called from idisplay, but for long strings we want to
   separate the code for the two cases.
   In idisplay it is called with displaymode true to indicate
   that the result should evaluate to itself. This means adding operations
   such as "reshape", "phrase", "fault" and "link" to the stream.

   The routine builds its result as a C string stored in Cbuffer.
   This technique greatly accelerates picture drawing as very few small
   arrays are created and destroyed.

   The routine computes the size of the object as it goes along.
   Items are either pushed or enough space is reserved so that
   storing them into the Cbuffer is safe.
*/

static int
disp(nialptr x, int displaymode)
{
  nialint     i,
              dsz = 0,
              tx = tally(x); /* tx is adjusted to 1 if 0 and displaymode */
  int         solitary;

  solitary = false;
  if (!atomic(x) && (displaymode || decor)) { /* add decoration needed at front */
    if (x == Null) {         /* treat Null as special case */
      reservechars(5);
      strcpy(ptrCbuffer, "Null");
      ptrCbuffer += 4;
      dsz += 4;
      freeup(x);
      return dsz;
    }
    if (tx == 0 || valence(x) != 1) { /* display the shape with a reshape at front */
      nialptr     shx;

      apush(x);              /* to protect x in ishape */
      apush(x);
      ishape();
      shx = apop();
      apop();                /* to unprotect x */
      dsz = disp(shx, true); /* display the shape, 2nd parameter is true to
                                force a Null shape to print as 'Null'. 
                                disp() frees the shx container */
      reservechars(10);
      strcpy(ptrCbuffer, " reshape ");  /* add the word reshape */
      ptrCbuffer += 9;
      dsz += 9;
      if (tx == 0)
      /* Use Null for shape */
      { reservechars(6);
        strcpy(ptrCbuffer, "Null ");  /* add the word Null */
        ptrCbuffer += 5;
        dsz += 5;
        freeup(x);
        return dsz;
      }
    }
    else 
    if (tx == 1 && kind(x) != chartype) {  /* display as solitary */
      solitary = true;
      pushch('[')
      dsz++;
    }
  }
  /* now display the list of items */
  switch (kind(x)) {
    case phrasetype:
    case faulttype:
        {
          nialint     len = tknlength(x);
          char       *cptr;

          if (displaymode || decor) {
            int         endcharfound = false;

            /* loop to search for internal character that would terminate a
             * constant phrase or fault denotation too soon */
            i = 0;
            cptr = pfirstchar(x); /* safe */
            while (!endcharfound && i++ < len) {
              char        ch = *cptr++;
              int         j;
              nialint     cnt = strlen(ENDCHARS);

              for (j = 0; j < cnt; j++)
                if (ch == ENDCHARS[j]) {
                  endcharfound = true;
                  break;
                }
            }
            if (endcharfound || (displaymode && len > MAXPGMLINE - 1))
              /* add operation to build the object */
            {
              reservechars(9);  /* room for ( and the op name */
              if (kind(x) == phrasetype) {
                strcpy(ptrCbuffer, "(phrase ");
                ptrCbuffer += 8;
                dsz += 8;
              }
              else {
                strcpy(ptrCbuffer, "( fault ");
                ptrCbuffer += 8;
                dsz += 8;
              }
              /* emit the string denotation for the phrase items */
              mkstring(pfirstchar(x));
              dsz += disp(apop(), displaymode); /* disp() frees the temporary */
              if (kind(x)==faulttype) /* add blanks to make canonical work */
              { pushch(' '); 
                dsz++;
              }
              pushch(')');   /* to close the the * construct */
              dsz++;
              break;
            }
            /* display phrase or fault symbol and fall through to the non-displaymode case */
            pushch((kind(x) == phrasetype ? '\"' : '?'));
            dsz++;
          }
          /* display the characters of the phrase or fault */
          if (len == 0) {    /* the phrase " or fault ? */
            *ptrCbuffer++ = BLANK;
            dsz++;
          }
          else {
            reservechars(len + 1);
            cptr = pfirstchar(x); /* safe */
            for (i = 0; i < len; i++)
              *ptrCbuffer++ = *cptr++;
            dsz += len;
          }
          break;
        }

    case booltype:
        {
          int         counter,
                      link_on = 0;

          if (displaymode) { /* Note tx >= 1 */
            nialint     len = tx;

            reservechars(6); /* to allow for link */
            if (len > MAXPGMLINE - 2) {
              strcpy(ptrCbuffer, "link ");
              ptrCbuffer += 5;
              dsz += 5;
              link_on = 1;
            }
            /* put it out in chunks to be linked so it does not overflow one
             * program text line */
            counter = 0;
            while (len > 0) {
              int         sectlen = (len > MAXPGMLINE - 2 ? MAXPGMLINE - 2 : len);

              for (i = 0; i < sectlen; i++) {
                pushch
                  (fetch_bool(x, i + (counter * (MAXPGMLINE - 2))) ? 'l' : 'o');
                dsz++;
              }
              if (link_on) {
                pushch(' ');
                dsz++;
              }
              len -= sectlen;
              counter++;
            }
            if (link_on) {
              ptrCbuffer--;  /* to remove last blank */
              dsz--;
            }
            break;
          }
          /* for sketch mode just put out the text string whatever its length */
          reservechars(tx + 1);
          for (i = 0; i < tx; i++)
            *ptrCbuffer++ = (fetch_bool(x, i) ? 'l' : 'o');
          dsz += tx;
        }
        break;
    case inttype:
        {
          for (i = 0; i < tx; i++) {
            char        buf[NDINT + 1];
            nialint     intlen;

            sprintf(buf, NIALINT_FORMAT, (nialint_format_type)fetch_int(x, i));
            reservechars(NDINT + 1);
            strcpy(ptrCbuffer, buf);
            ptrCbuffer += (intlen = strlen(buf));
            dsz += intlen;
            pushch(BLANK); /* add blank as separator */
            dsz++;
          }
          ptrCbuffer--;
          dsz--;
          break;
        }
    case realtype:
        {
          for (i = 0; i < tx; i++) {
            char        buf[NDREAL + 1];
            int         reallen;
            realtochar(fetch_real(x, i), buf);

            reservechars(NDREAL + 1);

            strcpy(ptrCbuffer, buf);

              ptrCbuffer += (reallen = strlen(buf));

            dsz += reallen;
            pushch(BLANK) /* add blank as separator */
              dsz++;
          }
          ptrCbuffer--;
          dsz--;

          break;
        }
    case chartype:
        {
          char       *cptr;

          if (atomic(x)) {
            if (charval(x) >= BLANK || charval(x) < 0) {
              reservechars(3);
              if (displaymode || decor) {
                *ptrCbuffer++ = GRAVE;
                dsz++;
              }
              *ptrCbuffer++ = charval(x);
              dsz++;
            }
            else if (displaymode || decor) {  /* put out (char N) */
              char        buf[20];  /* big enough if chars are Unicode */
              int         clen;

              reservechars(20);
              sprintf(buf, "(char %d)", charval(x));
              strcpy(ptrCbuffer, buf);
              ptrCbuffer += (clen = strlen(buf));
              dsz += clen;
            }
            else {           /* display an unprintable char as a blank in
                                nodecor mode */
              pushch(BLANK);
              dsz++;
            }
            break;
          }
          if (displaymode) { /* emit the string denotation in quotes with
                                internal quotes doubled and control chars
                                unchanged. Note tx >= 1 Break it into linked
                                chunks if too long. */
            nialint     len = tx,
                        quotecnt = 0,
                        i,
                        inuse;
            int         hasctrlchars = false;

            cptr = pfirstchar(x); /* safe, no allocation */
            for (i = 0; i < tx; i++) 
            { char ch = *(cptr+i);
               /* count quote marks to be added */
              if (ch == QUOTE)
                quotecnt++;
              if (ch >=0 && ch < BLANK)
                hasctrlchars = true;
            }
            if (hasctrlchars) {
              x = explode(x, valence(x), tally(x), 0, tally(x));
              /* frees original x, so disp() must always free x arg */
              dsz += disp(x, true);
              return dsz;
            }
            if (len + quotecnt > MAXPGMLINE - 2) {
              reservechars(6);  /* to allow for quotes and link */
              strcpy(ptrCbuffer, "link ");
              ptrCbuffer += 5;
              dsz += 5;
              inuse = 5;
            }
            else
              inuse = 0;
            /* put x out in chunks to be linked so it does not overflow one
             * program text line. Reserve all the space at once so that
             * Cbuffer won't move during the loop and hence ruin cptr. */

            reservechars(len + 1 + quotecnt + 3 * (len / MAXPGMLINE + 1));
            /* gets enough space for the internal quotes and the line breaks */
            cptr = pfirstchar(x); /* refetched to be safe */
            while (len > 0) {
              *ptrCbuffer++ = QUOTE;
              dsz++;
              inuse++;
              while (inuse < MAXPGMLINE - 2 && len > 0) {
                *ptrCbuffer++ = *cptr++;
                dsz++;
                inuse++;
                len--;
                if (*(ptrCbuffer - 1) == QUOTE) {
                  *ptrCbuffer++ = QUOTE;
                  dsz++;
                  inuse++;
                }
              }
              strcpy(ptrCbuffer, "\' ");
              ptrCbuffer += 2;
              dsz += 2;
              inuse = 0;
            }
            ptrCbuffer--;    /* to remove last blank */
            dsz--;
            break;
          }
          /* for sketch mode just put out the text string whatever its length */
          reservechars(tx + 3);
          if (decor) {
            *ptrCbuffer++ = QUOTE;
            dsz++;
          }
          cptr = pfirstchar(x); /* safe */
          for (i = 0; i < tx; i++) {
            char        ch = *cptr++;

            *ptrCbuffer++ = (ch >= 0 && ch < BLANK ? BLANK : ch);
            dsz++;
          }
          if (decor) {
            *ptrCbuffer++ = QUOTE;
            dsz++;
          }
          break;
        }

    case atype:
        {                    /* separate items using list notation for 
                                nonsimple arrays */
          int uselist = (displaymode && (tally(x)==0 || !simple(x)) && !solitary);
          if (uselist) {
            pushch('[')
              dsz++;
          }
          for (i = 0; i < tx; i++) {
            nialptr     it = fetch_array(x, i);

            dsz += disp(it, displaymode);
            reservechars(2);
            if (uselist)
              *ptrCbuffer++ = ',';
            else
              *ptrCbuffer++ = BLANK;
            dsz++;
#ifdef USER_BREAK_FLAG
            checksignal(NC_CS_OUTPUT);
#endif
          }
          ptrCbuffer--;      /* to remove the trailing blank or comma */
          dsz--;
          if (uselist) {
            pushch(']')
              dsz++;
          }
          break;
        }
  }
  if (solitary) {
    pushch(']')
      dsz++;
  }
  freeup(x);
  return dsz;
}

/* implements the Nial operation paste which gives the user control
   to paste text tables together.
*/


void
ipaste()
{
  nialptr     z,
              x,
              ta,
              y,
              vjust,
              hjust;
  nialint     ty,
              i;
  int         vpad,
              hpad,
              vlines,
              hlines;

  if (kind(top) == faulttype && top != Nullexpr &&
      top != Eoffault && top != Zenith && top != Nadir)
    return;
  z = apop();
  if (tally(z) != 2) {
    apush(makefault("?argument to paste must be a pair"));
    freeup(z);
    return;
  }
  splitfb(z, &x, &y);
  if (tally(x) != 6) {
    buildfault("control argument of paste needs 6 items");
    goto cleanup;
  }

  /* pick up and check vertical pad */
  ta = fetchasarray(x, 0);
  if (kind(ta) != inttype || valence(ta) != 0 || intval(x) < 0) {
    buildfault("vertical pad for paste must be integer");
    freeup(ta);
    goto cleanup;
  }
  vpad = intval(ta);
  freeup(ta);

/* pick up and check horizontal pad */
  ta = fetchasarray(x, 1);
  if (kind(ta) != inttype || valence(ta) != 0 || intval(x) < 0) {
    buildfault("horizontal pad for paste must be integer");
    freeup(ta);
    goto cleanup;
  }
  hpad = intval(ta);
  freeup(ta);

/* pick up and check vertical lines */
  ta = fetchasarray(x, 2);
  if (kind(ta) != inttype || valence(ta) != 0 || intval(x) < 0) {
    buildfault("vertical lines flag for paste must be integer");
    freeup(ta);
    goto cleanup;
  }
  vlines = intval(ta);
  freeup(ta);

/* pick up and check horizontal lines */
  ta = fetchasarray(x, 3);
  if (kind(ta) != inttype || valence(ta) != 0 || intval(x) < 0) {
    buildfault("horizontal lines flag for paste must be integer");
    freeup(ta);
    goto cleanup;
  }
  hlines = intval(ta);
  freeup(ta);

/* pick up and check vertical justification */
  ty = tally(y);
  vjust = fetchasarray(x, 4);
  if (kind(vjust) != inttype) {
    buildfault("vertical justification for paste must be int or array of ints");
    freeup(vjust);
    goto cleanup;
  }
  if (tally(vjust) != 1 && tally(vjust) != ty) {
    buildfault("vertical justification for paste must have one item or as many as the second argument");
    freeup(vjust);
    goto cleanup;
  }

/* pick up and check horizontal justification */
  hjust = fetchasarray(x, 5);
  if (kind(hjust) != inttype) {
    buildfault("horizontal justification for paste must be int or array of ints");
    freeup(hjust);
    goto cleanup;
  }
  if (tally(hjust) != 1 && tally(hjust) != ty) {
    buildfault("horizontal justification for paste must have one item or as many as the second argument");
    freeup(hjust);
    goto cleanup;
  }

  /* check validity of data argument */
  if (kind(y) != atype) {
    buildfault("second argument of paste must be an array of character tables");
    goto cleanup;
  }
  for (i = 0; i < ty; i++) {
    ta = fetch_array(y, i);
    if ((kind(ta) != chartype && tally(ta) != 0) || valence(ta) != 2) {
      buildfault("second argument of paste must be an array of character tables");
      goto cleanup;
    }
  }

  /* call the internal paste routine to do the work */
  apush(paste(y, vpad, hpad, vlines, hlines, vjust, hjust, false));
cleanup:
  freeup(x);                 /* x and y freed in case z is homogeneous */
  freeup(y);
  freeup(z);
}

#define deposit(ch)   *resptr++ = ch



/* paste formats atoms, arrays, etc. into boxes and draws the boxes.

   Arguments -
           x - array of character tables.
        vpad - inter-column pad. Tells how many blank columns inserted
               between data columns. If vlines, padding is done on both
               size of vertical lines.
        hpad - same for inter-row pad.
      vlines - switch telling if vertical lines are drawn.
      hlines - same for horizontal lines.
 (Note that if either one of vlines or hlines is on, the outer box is drawn.)
       vjust - map for vertical justification .  If same shape as x,
               then cells justified accordingly, if only one integer, then
               all cells justified the same. Flags are:
                  vtop   =  0 : top justification
                  vcenter=  1 : center    "
                  vbottom=  2 : bottom    "
       hjust - map for horizontal justification .  If same shape as x,
               then cells justified accordingly, if only one integer, then
               all cells justified the same. Flags are:
                  hleft  =  0 : left justification
                  hcenter=  1 : center    "
                  hright =  2 : right     "
     emptysw - switch telling whether to draw part of frame for empties.
*/
static      nialptr
paste(nialptr x, int vpad, int hpad, int vlines, int hlines, nialptr vjust, nialptr hjust, int emptysw)
{
  nialptr     nx,
              pRows,
              pCols,
              nvjust = Null,
              nhjust = Null,
              result,
              p1;
  nialint     tx,
              lim,
              vlim,
              xrows,
              xcols,
              item_rows,
              item_cols,
              index,
              j1,
              i1,
              i,
              j;
  int         vx,
              vesize,
              hesize,
              tres;
  char       *resptr;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "at top of PASTE \n");
  memchk();
#endif
/* the pasting algorithm:
   if valence > 2 then paste subarrays 2 dimensions at a time
   by spliting the array of diagrams of the items and calling
   paste recursively.
*/
  /* if a signal has been indicated goto top level */
  vx = valence(x);
  tx = tally(x);
  while (vx > 2) {
    int         nvx;

    vx -= 2;
    nx = nial_raise(x, vx);
    nvx = valence(nx);
    freeup(x);
    apush(vjust);            /* protect vjust */
    apush(hjust);            /* protect hjust */
    if (!atomic(vjust))
      nvjust = nial_raise(vjust, vx);
    if (!atomic(hjust))
      nhjust = nial_raise(hjust, vx);
    /* allocate container to hold pasted subarrays */
    x = new_create_array(atype, nvx, 0, shpptr(nx, nvx));
    tx = tally(x);
    if (tx == 0)
    {           /* attempting to paste array with no items.
                   return an array of shape 1 0 */
      nialptr     z;
      nialint     sh[2];

      sh[0] = 1;
      sh[1] = 0;
      z = new_create_array(atype, 2, 0, sh);
      freeup(x);
      freeup(nx);
      freeup(apop());        /* hjust */
      freeup(apop());        /* vjust */
      return (z);
    }
    /* loop to  recursively paste each of the subarrays of the rasied array */
    for (i = 0; i < tx; i++) {
      p1 = paste(fetch_array(nx, i), vpad, hpad, vlines, hlines,
                 atomic(vjust) ? vjust : fetch_array(nvjust, i),
                 atomic(hjust) ? hjust : fetch_array(nhjust, i), emptysw);
      store_array(x, i, p1);
    }
    freeup(nx);
    if (!atomic(vjust))
      freeup(nvjust);
    if (!atomic(hjust))
      freeup(nhjust);
    freeup(apop());          /* hjust */
    freeup(apop());          /* vjust */
    vlines = hlines = 0;
    vpad += 1;
    hpad += 2;
    vjust = avtop;
    hjust = ahleft;
#ifdef USER_BREAK_FLAG
    checksignal(NC_CS_OUTPUT);
#endif
  }

  /* problem now reduced to pasting a list or table of character tables.
   * Compute number of rows and columns.  */
  if (vx == 2) {
    xrows = pickshape(x, 0);
    xcols = pickshape(x, 1);
  }
  else {
    xrows = 1;
    xcols = tx;
  }

/*  produce: pRows and pCols, lists of integers containing
          maximum widths of rows and columns in x
*/
  {
    int         maxcols,
                maxrows,
                rows,
                cols;

    pRows = new_create_array(inttype, 1, 0, &xrows);
    vesize = (xrows != 0 || emptysw) && (hlines || vlines);
    for (i = 0; i < xrows; i++) {
      maxrows = (xcols == 0 ? emptysw : 0);
      for (j = 0; j < xcols; j++) {
        rows = pickshape(fetch_array(x, j + i * xcols), 0);
        if (maxrows < rows)
          maxrows = rows;
      }
      store_int(pRows, i, maxrows);
      vesize += vpad + maxrows;
      if (hlines || (i == xrows - 1 && vlines))
        vesize += vpad + 1;
      else if (i == xrows - 1)
        vesize -= vpad;
#ifdef USER_BREAK_FLAG
      checksignal(NC_CS_OUTPUT);
#endif
    }
    pCols = new_create_array(inttype, 1, 0, &xcols);
    hesize = (xcols != 0 || emptysw) && (hlines || vlines);
    for (j = 0; j < xcols; j++) {
      maxcols = (xrows == 0 ? emptysw : 0);
      for (i = 0; i < xrows; i++) {
        cols = pickshape(fetch_array(x, j + i * xcols), 1);
        if (maxcols < cols)
          maxcols = cols;
      }
      store_int(pCols, j, maxcols);
      hesize += hpad + maxcols;
      if (vlines || (j == xcols - 1 && hlines))
        hesize += hpad + 1;
      else if (j == xcols - 1)
        hesize -= hpad;
#ifdef USER_BREAK_FLAG
      checksignal(NC_CS_OUTPUT);
#endif
    }
  }

  /* create the container for the result and blank it */
  tres = vesize * hesize;
  {
    nialint     sh[2];

    sh[0] = vesize;
    sh[1] = hesize;
    result = new_create_array(chartype, 2, 0, sh);
  }
  resptr = pfirstchar(result);  /* safe */
  for (i = 0; i < tres; i++)
    deposit(BLANK);

  /* draw the lines */
  resptr = pfirstchar(result);  /* safe */
  if ((hlines || vlines) && (tres != 0)) {
    deposit(luc);
    for (j = 0; j < xcols; j++) {
      lim = fetch_int(pCols, j) + hpad;
      if (vlines || j == xcols - 1)
        lim += hpad;
      for (i = 0; i < lim; i++)
        deposit(hor);
      if (j == xcols - 1) {
        deposit(ruc);
      }
      else if (vlines) {
        deposit(ut);
      }
    }
  }
  for (i = 0; i < xrows; i++) {
    vlim = fetch_int(pRows, i) + vpad;
    if (hlines || i == xrows - 1)
      vlim += vpad;
    for (i1 = 0; i1 < vlim; i1++) {
      if (vlines || hlines) {
        deposit(ver);
        resptr += hpad;
      }
      for (j = 0; j < xcols; j++) {
        resptr += fetch_int(pCols, j) + hpad;
        if (vlines || (j == xcols - 1 && hlines)) {
          deposit(ver);
          resptr += hpad;
        }
      }
      resptr -= hpad;
    }
    if (hlines || (i == xrows - 1 && vlines)) {
      if (i == xrows - 1)
        deposit(llc);
      else
        deposit(gt);
      for (j = 0; j < xcols; j++) {
        lim = fetch_int(pCols, j) + hpad;
        if (vlines || j == xcols - 1)
          lim += hpad;
        for (j1 = 0; j1 < lim; j1++)
          deposit(hor);
        if (j == xcols - 1) {
          if (i == xrows - 1)
            deposit(rlc);
          else
            deposit(rt);
        }
        else if (vlines) {
          if (i == xrows - 1)
            deposit(lt);
          else
            deposit(cro);
        }
      }
    }
  }

  /* put the data. index gives the position in the result as an offset.
   * resptr is a pointer to the position.  */

  index = 0;
  if (hlines || vlines)
    index += hesize * (vpad + 1);
  for (i = 0; i < xrows; i++) {
    if (vlines || hlines)
      index += hpad + 1;
    for (j = 0; j < xcols; j++) {
      nialptr     an_item = fetch_array(x, i * xcols + j);
      char       *itemptr = pfirstchar(an_item);  /* safe */

      item_rows = pickshape(an_item, 0);
      item_cols = pickshape(an_item, 1);
      resptr = pfirstchar(result) + index;  /* safe */
      switch (atomic(vjust) ? intval(vjust) : fetch_int(vjust, i * xcols + j)) {
        case vtop:
            break;
        case vcenter:
            resptr += hesize * ((fetch_int(pRows, i) - item_rows) / 2);
            break;
        case vbottom:
            resptr += hesize * (fetch_int(pRows, i) - item_rows);
            break;
      }
      switch (atomic(hjust) ? intval(hjust) : fetch_int(hjust, i * xcols + j)) {
        case hleft:
            break;
        case hcenter:
            resptr += (fetch_int(pCols, j) - item_cols) / 2;
            break;
        case hright:
            resptr += fetch_int(pCols, j) - item_cols;
            break;
      }
      for (i1 = 0; i1 < item_rows; i1++) {
        strncpy(resptr, itemptr, item_cols);
        resptr += hesize;
        itemptr += item_cols;
      }
      index += fetch_int(pCols, j) + hpad;
      if (j == xcols - 1)
      { if (vlines || hlines)
          index += 1;
        else
          index -= hpad;
      }
      else if (vlines)
        index += hpad + 1;
    }
    index += hesize * (fetch_int(pRows, i) + vpad - 1);
    if (hlines)
      index += hesize * (vpad + 1);
#ifdef USER_BREAK_FLAG
    checksignal(NC_CS_OUTPUT);
#endif
  }
  if (hlines && vlines) {    /* add decorations */
    if (vx == 0)             /* a non-atomic single */
      store_char(result, 0, 'o');
  }
  freeup(x);
  freeup(vjust);
  freeup(hjust);
  freeup(pRows);
  freeup(pCols);
#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "at bottom of PASTE \n");
  memchk();
#endif
  return (result);
}


/* computes the positions of items in the picture of a single, list
   or table. (The extension to higher valent arrays is best done at
   the Nial level).
*/
void
ipositions()
{
  nialptr     x;
  nialint     tx;
  int         vx,
              kx,
              lsw,
              hspace;

  x = apop();
  vx = valence(x);
  tx = tally(x);
  kx = kind(x);
  if (tx == 0)
  { if (vx > 1) 
    { apush(x);
      ishape();
      apush(Null);
      b_reshape();
    }
    else {
      apush(Null);
      freeup(x);
    }
  }
  else 
  if (vx == 0) 
  { if (atomic(x))
      pair(Zero, Zero);
    else
      pair(One, One);
    isingle();
    freeup(x);
  }
  else if (valence(x) <= 2) {
    if (sketch) {
      int_each(isketch, x);
      lsw = !(simple(x) && tally(x) > 0);
      hspace = (!lsw && (kx == chartype || kx == booltype) ? 0 : !lsw);
    }
    else {
      int_each(idiagram, x);
      lsw = true;
      hspace = !lsw;
    }
    pospaste(apop(), 0, hspace, lsw, lsw);
  }
  else {
    apush(makefault("?valence too high in positions"));
    freeup(x);
  }
}

/* this routine mimics the action of paste for valence 1 or 2; but
  it just computes positions instead of doing the paste. */

static void
pospaste(nialptr x, int vpad, int hpad, int vlines, int hlines)
{
  nialptr     pRows,
              pCols;
  nialint     xrows,
              xcols,
              i,
              j;
  int         vesize,
              hesize,
              start,
              vx,
              maxcols,
              maxrows,
              rows,
              cols;

  vx = valence(x);
  if (vx == 2) {
    xrows = pickshape(x, 0);
    xcols = pickshape(x, 1);
  }
  else {
    xrows = 1;
    xcols = pickshape(x, 0);
  }

/* produce pRows and pCols, lists of starting points
   of rows and columns in x               */

  start = (hlines || vlines);
  vesize = start;
  pRows = new_create_array(inttype, 1, 0, &xrows);
  store_int(pRows, 0, start);
  for (i = 0; i < xrows - 1; i++) {
    maxrows = 0;
    for (j = 0; j < xcols; j++) {
      rows = pickshape(fetch_array(x, j + i * xcols), 0);
      if (maxrows < rows)
        maxrows = rows;
    }
    vesize += vpad + maxrows;
    if (hlines)
      vesize += vpad + 1;
    store_int(pRows, i + 1, vesize);
#ifdef USER_BREAK_FLAG
    checksignal(NC_CS_OUTPUT);
#endif
  }
  hesize = start;
  pCols = new_create_array(inttype, 1, 0, &xcols);
  store_int(pCols, 0, start);
  for (j = 0; j < xcols - 1; j++) {
    maxcols = 0;
    for (i = 0; i < xrows; i++) {
      cols = pickshape(fetch_array(x, j + i * xcols), 1);
      if (maxcols < cols)
        maxcols = cols;
    }
    hesize += hpad + maxcols;
    if (vlines)
      hesize += hpad + 1;
    store_int(pCols, j + 1, hesize);
  }
  pair(pRows, pCols);
  icart();
  if (vx == 1)
    ilist();
  freeup(x);
}
