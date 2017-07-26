/* ==============================================================

   MODULE     SCAN.C

  COPYRIGHT NIAL Systems Limited  1983-2016

   This module implements the scanner, descanner and deparser primitives.

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

/* MATHLIB */
#include <math.h>

/* CLIB */
#include <ctype.h>


/* Q'Nial header files */

#include "scan.h"
#include "lexical.h"
#include "token.h"
#include "states.h"
#include "roles.h"
#include "qniallim.h"
#include "absmach.h"
#include "basics.h"
#include "lib_main.h"
#include "if.h"

#include "symtab.h"          /* for symtab macros */
#include "parse.h"           /* for t_tokenstream */
#include "utils.h"           /* for cnvtup */
#include "reserved.h"        /* macros for reserved words */
#include "fileio.h"          /* for OF_DEBUG code */
#include "blders.h"          /* for get routines */
#include "getters.h"         /* for get macros */

/* The Nial scanner is implemented as a finite state machine.
   It uses the tables below to do state transitions.
*/ 

static void mktoken(char *buffer, int state);

static nialptr scan(nialptr x);
static void startline(int n);
static void finishline(void);


#define     CharacterCnt  256

/* the transition table indicates the state moved to from a given state and character class. */

static char transition[StateCnt][ClassCnt] =
/*         L   Di  De  U   Do  Un  Ft  Ph  Q   C   Cl  Eq  El  Le  Bl  Ti  Ds  Se  Sm  Lj  Lt  Gt  Lb  cG  cC  Pl*/
 /* St */ {{I, N, Dm, St, D, I, F, P, S, Co, Ge, Sy, Fi, I, St, M, Da, Dm, Sy, I, Lo, Hi, Bo, Ch, St, Sy},
 /* I */ {I, I, A, A, A, I, A, A, A, A, A, A, A, I, A, A, A, A, A, I, A, A, I, A, A, A},
 /* Co */ {Co, Co, Co, Rp, Co, Co, Co, Co, Co, Co, Co, Co, A, Co, Co, Co, Co, A, Co, Co, Co, Co, Co, Co, Co, Co},
 /* S */ {S, S, S, S, S, S, S, S, S1, S, S, S, E1, S, S, S, S, S, S, S, S, S, S, S, S, S},
 /* S1 */ {A, A, A, A, A, A, A, A, S, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A},
 /* P */ {P, P, A, Rp, P, P, P, P, P, P, P, P, A, P, A, P, P, A, P, P, P, P, P, P, P, P},
 /* N */ {A, N, A, A, R1, A, A, A, A, A, A, A, A, R2, A, A, A, A, A, C0, A, A, A, A, A, A},
 /* D */ {A, R1, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A},
 /* R1 */ {A, R1, A, A, E9, A, A, A, A, A, A, A, A, R2, A, A, A, A, A, C0, A, A, A, A, A, A},
 /* R2 */ {E3, R4, E3, E3, E9, E3, E3, E3, E3, E3, E3, E3, E3, E3, E3, R3, R3, E3, E3, C0, E3, E3, E3, E3, E3, R3},
 /* R3 */ {E3, R4, E3, E3, E9, E3, E3, E3, E3, E3, E3, E3, E3, E3, E3, E3, E3, E3, E3, C0, E3, E3, E3, E3, E3, E3},
 /* R4 */ {A, R4, A, A, E9, A, A, A, A, A, A, A, A, A, A, A, A, A, A, C0, A, A, A, A, A, A},
 /* F */ {F, F, A, Rp, F, F, F, F, F, F, F, F, A, F, A, F, F, A, F, F, F, F, F, F, F, F},
 /* Sy */ {A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A},
 /* C0 */ {E4, C2, E4, E9, C3, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, C1, C1, E4, E4, E4, E4, E4, E4, E4, E4, E4},
 /* C1 */ {E4, C2, E4, E9, C3, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4, E4},
 /* C2 */ {A, C2, A, E9, C4, A, A, A, A, A, A, A, A, C5, A, A, A, A, A, A, A, A, A, A, A, A},
 /* C3 */ {E5, C4, E5, E9, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5, E5},
 /* C4 */ {A, C4, A, E9, A, A, A, A, A, A, A, A, A, C5, A, A, A, A, A, A, A, A, A, A, A, A},
 /* C5 */ {E6, C7, E6, E9, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, C6, C6, E6, E6, E6, E6, E6, E6, E6, E6, C6},
 /* C6 */ {E6, C7, E6, E9, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6, E6},
 /* C7 */ {A, C7, A, E9, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A},
 /* Dn */ {A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A},
 /* Ge */ {A, A, A, A, A, A, A, A, A, A, A, Ge, A, A, A, A, A, A, A, A, A, A, A, A, A, A},
 /* Lo */ {A, A, A, A, A, A, A, A, A, A, A, Lo, A, A, A, A, A, A, A, A, Lo, A, A, A, A, A},
 /* Hi */ {A, A, A, A, A, A, A, A, A, A, A, Hi, A, A, A, A, A, A, A, A, A, Hi, A, A, A, A},
 /* Ue */ {A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A},
 /* M */ {A, A, A, A, Td, A, A, A, A, A, A, Ue, A, A, A, A, A, A, A, A, A, A, A, A, A, A},
 /* Td */ {E2, R1, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2, E2},
 /* Da */ {A, N, A, A, Td, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A},
 /* Bo */ {I, I, A, A, A, I, A, A, A, A, A, A, A, I, A, A, A, A, A, I, A, A, Bo, A, A, A},
 /* Ch */ {Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc, E8, Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc, Fc},
 /* Fc */ {A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A}
};


/* the table documenting the class of each character */

static char class1[CharacterCnt] =

{El, U, U, U, U, U, U, U,    /* 0  */
  U, Bl, El, U, U, U, U, U,  /* 8  */
  U, U, U, U, U, U, U, U,    /* 16  */
  U, U, U, U, U, U, U, U,    /* 24  */
  Bl, Sm, Ph, De, Sm, C, L, Q,  /* 32  */
  De, De, Sm, Pl, De, Ds, Do, Sm, /* 40  */
  Di, Di, Di, Di, Di, Di, Di, Di, /* 48  */
  Di, Di, Cl, Se, Lt, Eq, Gt, Ft, /* 56  */
  Sm, L, L, L, L, Le, L, L,  /* 64  */
  L, L, Lj, L, Lb, L, L, Lb, /* 72  */
  L, L, L, L, L, L, L, L,    /* 80  */
  L, L, L, De, Sm, De, Sm, Un,  /* 88  */
  cG, L, L, L, L, Le, L, L,  /* 96  */
  L, L, Lj, L, Lb, L, L, Lb, /* 104 */
  L, L, L, L, L, L, L, L,    /* 112 */
  L, L, L, De, Sm, De, Ti, cC/* 120 */
  ,L, L, L, L, L, L, L, L,   /* 128 */
  L, L, L, L, L, L, L, L,    /* 136 */
  L, L, L, L, L, L, L, L,    /* 144 */
  L, L, L, L, L, L, L, L,    /* 152 */
  L, L, L, L, L, L, L, L,    /* 160 */
  cC, L, L, L, L, cC, cC, cC,/* 168 */
  cC, cC, cC, cC, cC, cC, cC, cC, /* 176 */
  cC, cC, cC, cC, cC, cC, cC, cC, /* 184 */
  cC, cC, cC, cC, cC, cC, cC, cC, /* 192 */
  cC, cC, cC, cC, cC, cC, cC, cC, /* 200 */
  cC, cC, cC, cC, cC, cC, cC, cC, /* 208 */
  cC, cC, cC, cC, cC, cC, cC, cC, /* 216 */
  L, L, L, L, L, L, L, L,    /* 224 */
  L, L, L, L, L, L, L, L,    /* 232 */
  L, L, L, L, L, L, L, L,    /* 240 */
  L, L, L, L, L, L, cC, U,   /* 248 */
};


#ifdef VMCMS

/* code to support EBCIDIC to ASCII trnaslation for the IBM VMCMS mainfrome version. */

char        ebctoasc[256] = {
  '\000', '\001', '\002', '\003', '\024', '\011', '\377', '\377',
  '\072', '\072', '\072', '\013', '\014', '\015', '\016', '\017',
  '\020', '\021', '\022', '\023', '\072', '\012', '\010', '\000',
  '\030', '\031', '\072', '\072', '\034', '\035', '\036', '\037',
  '\072', '\072', '\034', '\072', '\072', '\012', '\027', '\033',
  '\072', '\072', '\072', '\072', '\072', '\005', '\006', '\007',
  '\072', '\072', '\026', '\072', '\072', '\036', '\072', '\004',
  '\072', '\072', '\072', '\072', '\024', '\025', '\072', '\032',
  '\040', '\072', '\072', '\072', '\072', '\072', '\072', '\072',
  '\072', '\072', '\072', '\056', '\074', '\050', '\053', '\174',
  '\046', '\072', '\072', '\072', '\072', '\072', '\072', '\072',
  '\072', '\072', '\041', '\044', '\052', '\051', '\073', '\136',
  '\055', '\057', '\072', '\072', '\072', '\072', '\072', '\136',
  '\004', '\176', '\174', '\054', '\045', '\137', '\076', '\077',
  '\072', '\072', '\072', '\072', '\072', '\072', '\072', '\072',
  '\072', '\140', '\072', '\043', '\100', '\047', '\075', '\042',
  '\072', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
  '\150', '\151', '\072', '\173', '\072', '\072', '\072', '\072',
  '\072', '\152', '\153', '\154', '\155', '\156', '\157', '\160',
  '\161', '\162', '\072', '\175', '\072', '\072', '\072', '\072',
  '\072', '\176', '\163', '\164', '\165', '\166', '\167', '\170',
  '\171', '\172', '\072', '\072', '\072', '\133', '\072', '\072',
  '\021', '\072', '\072', '\072', '\072', '\072', '\072', '\072',
  '\072', '\072', '\072', '\072', '\072', '\135', '\072', '\072',
  '\173', '\101', '\102', '\103', '\104', '\105', '\106', '\107',
  '\110', '\111', '\072', '\072', '\072', '\072', '\072', '\072',
  '\175', '\112', '\113', '\114', '\115', '\116', '\117', '\120',
  '\121', '\122', '\072', '\072', '\072', '\072', '\072', '\373',
  '\134', '\072', '\123', '\124', '\125', '\126', '\127', '\130',
  '\131', '\132', '\072', '\072', '\072', '\072', '\072', '\072',
  '\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
'\070', '\071', '\072', '\072', '\072', '\072', '\072', '\072'};

#endif



/* local global variables  */

static nialptr tknstream;
static nialint tokencnt;
static nialint tslimit;

/* prototypes */

static void cleartokenstream(void);
static void addtkn(nialptr x);
static void cleanuptokenstream(void);
static nialptr finishtokenstream(void);

/* the token  array support routines */

/* Tokens are stored in an array as a pair of consecutive items.
   The first item is the token property and the second is the text as a phrase.
   We do not store the pair as an array to avoid excessive array creation and deletion. */

/* routine to set up an empty token array */

static void
cleartokenstream()
{
  tslimit = TOKENSTREAMSIZE;
  tknstream = new_create_array(atype, 1, 0, &tslimit);
  tokencnt = 0;
}

/* routine to add a token to the token array allowing for expansion if necessary. */

static void
addtkn(nialptr x)
{
  nialptr     newtknstream;

  if (tokencnt == tslimit) {
    tslimit = tslimit + TOKENSTREAMSIZE;
    newtknstream = new_create_array(atype, 1, 0, &tslimit);
    copy(newtknstream, 0, tknstream, 0, tokencnt);
    freeup(tknstream);
    tknstream = newtknstream;
  }
  store_array(tknstream, tokencnt, x);
  tokencnt += 1;
}


/* routine to clean up the token array when error occurs */

static void
cleanuptokenstream()
{
  nialint     k;

  for (k = 0; k < tokencnt; k++)
    freeup(fetch_array(tknstream, k));
  freeup(tknstream);         /* get rid of token array */
}

/* routine to create array of size tokencnt and push it on the stack */

static      nialptr
finishtokenstream()
{
  nialptr     res;
  int         i;

  res = new_create_array(atype, 1, 0, &tokencnt);
  for (i = 0; i < tokencnt; i++)
    store_array(res, i, fetch_array(tknstream, i));
  freeup(tknstream);         /* get rid of token array */
  return (res);
}


/*
   The routine "scan"  scans the line buffer for NIAL tokens.  The tokens found are
   stored in a token array for further processing.  It is a table driven scanner
   and therefore can be easily modified by altering the tables found above.
*/


static      nialptr
scan(nialptr x)
{
  nialptr     k;
  nialint     i,  /* start of current token in buffer */
              j,  /* end of current token in buffer */
              length;
  int         state,
              laststate,
              chrclass,
              character;
  char       *mess,
             *buffer;

  length = tally(x) + 1;

  cleartokenstream();   /* initialize token array */

  addtkn(createint(t_tokenstream));  /* add the tag */

  /* initialize loop */
  i = 0;
  j = 0;
  state = sStart;

  /* loop picking up tokens until we hit the Finish state or exhaust the string */

  while (state != sFinish && j < length) {
    buffer = pfirstchar(x);  /* safe: restored after a create in the heap. */
    character = buffer[j];

    /* determine the class of the input character */

    if (character < 0)       /* IBM > 128 characters screw up class table
                              * conv. */
      character = BLANK;
    chrclass = class1[character];

    /* determine the state transition */
    laststate = state;
    state = transition[laststate][chrclass];

    /* if in start state move token start indicator */
    if (state == sStart)
      i++;

    /* if state is Replace put in a blank and assume previous state */
    if (state == sReplace) {
      buffer[j] = BLANK;
      state = laststate;
      j++;
    }

    else /* check if an error state */
    if ((state >= MINerror) && (state <= MAXerror)) { 
      char        inbuf[100]; /* length of scan error message is always  < 100 */

      /* create appropriate error message */
      inbuf[0] = '\0';
      strcat(inbuf, "?SCAN - ");
      switch (state) {
        case sError1:
            mess = "missing ' at end of string: ";
            break;
        case sError2:
            mess = "fractional part of real number missing: ";
            break;
        case sError3:
            mess = "exponent of real number missing: ";
            break;
        case sError4:
            mess = "imaginary part of complex number missing: ";
            break;
        case sError5:
            mess = "fractional part of imaginary number missing: ";
            break;
        case sError6:
            mess = "exponent of imaginary number missing: ";
            break;
        case sError7:
            mess = "undefined character after ` : ";
            break;
        case sError8:
            mess = "missing character after ` : ";
            break;
        case sError9:
            mess = "extra period in number after ";
            break;
        default:
            mess = "";       /* to avoid gcc warning */
      }
      {
        int         buflength;

        strcat(inbuf, mess);
        buflength = strlen(inbuf);
        for (k = 0; k < (j - i < 6 ? j - i : 6); k++) /* 6 = number of chars
                                                       * to show */
          inbuf[k + buflength] = buffer[i + k];
        inbuf[k + buflength] = '\0';
        cleanuptokenstream();

        /* create the fault and return from scan */
        return (createatom(faulttype, inbuf));  /* use createatom to avoid a trigger */
        
      }
    }

    else /* not an error state */

    if (state != sAccept)  /* move end of token indicator */
      j++;

    else {  /* construct the token */
      char        svchar = buffer[j];

      buffer[j] = '\0';      /* insert terminating null char */
      mktoken(&buffer[i], laststate);
      buffer = pfirstchar(x);/* safe: reloaded here in case x moved during mktoken */
      buffer[j] = svchar;

      /* initialize to scan the next token */
      state = sStart;
      i = j;

      checksignal(NC_CS_NORMAL);  /* to allow a user interrupt */
    }
  }
  return (finishtokenstream());
}


 /* Adds a token to the token array as two consecutive items:
    1. the token property
    2. the token text as a phrase.
*/

static void
mktoken(char *tknstring, int state)
{
  nialptr     print_value,
              entr;

  /* determine action based on previous state when token is accepted */

  switch (state) {
    case sComment:
        addtkn(createint(commentprop));

        /* remove trailing blanks in a comment */
        { int i = 1, len = strlen(tknstring);
          while (tknstring[len - i] == BLANK)
            i++;
          if (i>1)
            tknstring[len - i + 1] = '\0';
        }
        addtkn(makephrase(tknstring));
        break;

    case sIdentifier:
    case sSymbol:
    case sLower:
    case sHigher:
    case sUnequal:
    case sDash:
    case sMacron:
        {  /* store token in upper case */
          char        temp[MAXIDSIZE + 1];

          strncpy(temp, tknstring, MAXIDSIZE);
          temp[MAXIDSIZE] = '\0'; /* in case null not found */
          cnvtup(temp);
          print_value = makephrase(temp);

          /* look for reserved word in system symtab */
          entr = Blookup(get_root(global_symtab), print_value);

          if (entr != notfound && sym_role(entr) == Rres) /* found reserved word */
            addtkn(createint(delimprop));
          else
            addtkn(createint(identprop)); /* assumed to be an identifier */
          addtkn(print_value);
          break;
        }

    case sDelim:
    case sDot:
    case sGets: /* create the delimiter token */
        addtkn(createint(delimprop));
        addtkn(makephrase(tknstring));
        break;
    default:
        /* all other cases are constants. We encode the type in the token property */
        addtkn(createint(constprop + state));
        addtkn(makephrase(tknstring));
        break;
  }
}


/* iscan implements the Nial level scan routine that uses the internal scanner */

void
iscan()

{
  nialptr     buffer;

  if (kind(top) == faulttype)
    return;
  if (kind(top) == phrasetype)
    istring();
  ilist();
  buffer = apop();
  if (tally(buffer) == 0) {  /* result is a tokenstream with just the header */
    apush(createint(t_tokenstream));
    isolitary();
  }
  else if (kind(buffer) != chartype)  /* must be a string */
    buildfault("type error in scan");
  else
    apush(scan(buffer));  /* call the C level scanner and push the result */
  freeup(buffer);
}

/* routines for the descanner */

static int  indent;          /* indentation amount in descan */
static int  lncount;         /* number of lines pushed */

#define charcnt (ptrCbuffer - startCbuffer)

/* indicates number of character already put out to the current line */


/* descan takes a Nial array of tokens and puts the print values into a list 
   of character strings which is returned on the stack.
   Identifiers are given in upper case, in lower case or beginning with upper
   case according to their type. One blank is inserted between tokens.
   Linefeeds and indenting are put in upon reading tokens with properties
   indent, exdent or eol, and if length of current line is >= descanlinelength.
   Each token occupies two items of the stream with the property preceding
   the printvalue. Some tokens are formatting ones.

   Each line is assembled in the Cbuffer. The routine finishline() is
   used to do the cleanup of one line and preparation for the next.
   Startline does the indentation for a new line. The code allows for long
   tokens like comments and strings that may stretch across multiple lines.
*/

static int  emptyline;       /* flag to indicate an empty line */

static void
descan(nialptr tknstrm)
{
  nialptr     printval;
  nialint     ntkns,
              i,
              j,
              lenpvt;
  int         prop,
              oldprop = delimprop;

  lncount = 0;
  startline(0);
  ntkns = tally(tknstrm);    /* only called with ntkns > 1 */
  indent = 1;                /* start indenting with one on subsequent lines */
  j = 1;
  if (ntkns==1)
  { apush(Voidstr);
    return;
  }
  while (j < ntkns - 1) {    /* -1 to allow for double increment in loop */
    prop = intval(fetch_array(tknstrm, j));
    j++;
    printval = fetch_array(tknstrm, j);
    j++;
    lenpvt = tknlength(printval);
    if (prop > constprop)
      prop = constprop;      /* to fall into normal case */
    
    /* select based on token property */
    switch (prop) {
      case (delimprop):
      case (identprop):
      case (constprop):
          /* main case */
          if (charcnt + lenpvt >= MAXPGMLINE)
            /* not enough room for token on the rest of the line */
          {
            if (emptyline) { /* the token should fit if there is less indentation */
              int         diff = lenpvt - MAXPGMLINE;

#ifdef DEBUG
              if (diff > indent) {
                nprintf(OF_DEBUG, "token bigger than expected\n");
                nabort(NC_ABORT);
              }
#endif
              if (diff > indent)  /* just in case the token is too big */
                diff = indent - 1;
              ptrCbuffer -= diff; /* removes diff blanks at beginning */
            }

            else { /* not empty line */
              finishline();
              /* indent new line unless token won't fit */
              startline(lenpvt + indent < MAXPGMLINE ? indent : 1);
            }
          }

          /* loop to put the entire token on the current line */
          i = 0;
          while (i < lenpvt) {
            char        ch;

            ch = tknchar(printval, i);
            pushch(ch);
            i++;
          }
          pushch(BLANK);     /* to separate tokens */
          emptyline = false;

          /* handle moving comment boundary to a new line */
          if (prop == delimprop && oldprop == commentprop) {
            finishline();
            startline(indent);
          }
          break;
      case (commentprop):
          {
            int         done = false;

            /* put out comments with a standard indentation avoiding having
               them split in the middle of words. */
            if (!emptyline)  /* finish the previous line before the comment */
              finishline();
            startline(commentamt);  /* make the comment line start at a
                                     * standard indent; */
            i = 0;
            while (!done) {  /* loop needed for multiple line comments. 
                                skip blanks before next word */
              char        ch;
              int         j,
                          k;

              while ((ch = tknchar(printval, i)) == BLANK)
                i++;

              /* look for end of word or token */
              j = i;
              ch = tknchar(printval, j);
              while (j < lenpvt && ch != BLANK) {
                j++;
                ch = tknchar(printval, j);
              }

              if (charcnt + j - i + 1 >= MAXPGMLINE - 1) {
                finishline();
                startline(commentamt + 2);  /* subsequent comment lines
                                               have indent 2 */
              }

              /* the word will fit */
              if (!emptyline)
                pushch(BLANK);  /* put in the blank separator */

              /* loop to put out the token */
              for (k = i; k < j; k++) {
                ch = tknchar(printval, k);
                pushch(ch);
              }
              i = j;
              emptyline = false;
              done = i >= lenpvt;
            }
            pushch(BLANK);     /* to separate tokens */
          }
          break;

      case (eolprop):        /* start a new line */
          finishline();
          startline(indent);
          break;

      case (indentprop):     /* increase indentation */
          indent += indentamt;
          break;

      case (exdentprop):     /* decrease indentation */
          indent -= indentamt;
          break;

#ifdef DEBUG
      default:
          nprintf(OF_DEBUG, "invalid case in descan %d\n", prop);
          nabort(NC_ABORT);
#endif
    }
    oldprop = prop;
  }

  if (!emptyline)
    --ptrCbuffer;  /* to remove the last BLANK */
  finishline();
  
/* make the list of lines */
  mklist(lncount);  /* pushes the result */
}


/* finishline creates an array to store the items pushed onto
   Cbuffer and then pushes it on the Nial stack and counts it.
   It resets the Cbuffer for the next line.
*/

static void
finishline()
{
#ifdef DEBUG
  if (charcnt == 0) {        /* no content on this line. This shouldn't happen */
    nprintf(OF_DEBUG, "empty line produced by descan\n");
    nabort(NC_ABORT);
  }
  else
#endif

  {
    nialint     length = charcnt;
    char       *p = startCbuffer;

    pushch('\0');            /* terminate the string in Cbuffer */

    /* check that the line has content */
    while (*p == BLANK)
      p++;
    if (*p != '\0') { /* create the string and fill it */
      nialptr     line = new_create_array(chartype, 1, 0, &length);

      strcpy(pfirstchar(line), startCbuffer); /* copy it to array */
      apush(line);
      lncount++;             /* count the line */
    }
  }
}

/* does an indent for a new line to n */

static void
startline(int n)
{
  int         i;

  ptrCbuffer = startCbuffer; /* set the line buffer */
  reservechars(n + 1);
  for (i = 0; i < n; i++)
    pushch(BLANK);
  emptyline = true;
}


/* implements the Nial primitiive that converts a character array
   to upper case. Note that this is not a pervasive operation */

void
itoupper()
{
  nialptr     x = apop();
  int         v = valence(x);
  nialint     t = tally(x);

  /* if arg is empty return it */
  if (t == 0) {
    apush(x);
    return;
  }

  if (kind(x) == chartype) { /* argument is a string or character */
    nialptr     z;
    nialint     i;

    /* create the result container */
    z = new_create_array(chartype, v, 0, shpptr(x, v));

    /* loop to convert to upper case. Also removes negative control code chars */
    for (i = 0; i < t; i++)
    { char c = fetch_char(x,i);
      store_char(z, i, Upper((int)c));
    }
    apush(z);
  }
  else
    apush(makefault("?not char data in toupper"));
  freeup(x);
}

/* implements the Nial primitiive that converts a character array
   to lower case. Note that this is not a pervasive operation */

void
itolower()
{
  nialptr     x = apop();
  int         v = valence(x);
  nialint     t = tally(x);

  /* if arg is empty return it */
  if (t == 0) {
    apush(x);
    return;
  }
  if (kind(x) == chartype) { /* argument is a string or character */
    nialptr     z;
    nialint     i;

    /* create the result container */
    z = new_create_array(chartype, v, 0, shpptr(x, v));

    /* loop to convert to lower case. Also removes negative control code chars */
    for (i = 0; i < t; i++)
    { char c = fetch_char(x,i);
      store_char(z, i, Lower((int)c));
    }
    apush(z);
  }
  else
    apush(makefault("?not char data in tolower"));
  freeup(x);
}


/* routines to support the deparser */

/* prototypes for local deparser routines */

static void finishline(void);
static void startline(int);
static nialptr mapident(nialptr pv, int role);
static void descan(nialptr tknstrm);
static void deparse(nialptr br);
static void popeol(void);
static void emitrwd(char *reswrd);
static void emiteol(void);
static void emitincrin(void);
static void emitdecrin(void);


/* routine to map an identifier to its canonical form:
      variable:      first letter UC, rest in LC
      expression:    first letter UC, rest in LC
      operation:     LC
      transformer:   UC
      reserved word: UC 
   Uses Cbuffer to hold it as it is built.
*/

static      nialptr
mapident(nialptr pv, int role)
{
  nialptr     npv;
  nialint     n,
              i;
  int         ch;

  /* initialize Cbuffer and reserve space */
  n = tknlength(pv);
  ptrCbuffer = startCbuffer;
  reservechars(n + 1);

  /* pick up first char and treat according to role */
  ch = tknchar(pv, 0);
  switch (role) {
    case Rres:
        ch = Upper(ch);
        break;
    case Roptn:
        ch = Lower(ch);
        break;
    case Rexpr:
    case Rconst:
    case Rvar:
    case Rident:
    case Rtrans:
        ch = Upper(ch);
        break;
  }
  pushch(ch);

  /* loop over remaining characters and treat according to role */
  for (i = 1; i < n; i++) {
    ch = tknchar(pv, i);
    switch (role) {
      case Rres:
          ch = Upper(ch);
          break;
      case Roptn:
      case Rexpr:
      case Rconst:
      case Rvar:
      case Rident:
          ch = Lower(ch);
          break;
      case Rtrans:
          ch = Upper(ch);
          break;
    }
    pushch(ch);
  }

  /* fudge here to make Boolean atoms have output in lower case. */
  if (n == 1 && ch == 'L')
    *startCbuffer = 'l';
  if (n == 1 && ch == 'O')
    *startCbuffer = 'o';
  pushch('\0');
  npv = makephrase(startCbuffer);
  return (npv);
}

/* routine to remove last token pushed if an eol */

static void
popeol()
{
  if (intval(fetch_array(tknstream, tokencnt - 2)) == eolprop)
    tokencnt -= 2;
}

/* routine to emit a reserved word token */

static void
emitrwd(char *reswrd)
{
  addtkn(createint(delimprop));
  addtkn(makephrase(reswrd));
}

/* routine to emit an eol token */

static void
emiteol()
{
  addtkn(createint(eolprop));
  addtkn(Typicalphrase);
}

/* routine to emit an increase indent token */

static void
emitincrin()
{
  addtkn(createint(indentprop));
  addtkn(Typicalphrase);
}

/* routine to emit a decrease indent token */

static void
emitdecrin()
{
  addtkn(createint(exdentprop));
  addtkn(Typicalphrase);
}


/* Deparse takes as input a parse tree. It walks the tree recursively
   pushing tokens corresponding to the structure of the tree.

   Tokens with properties indent exdent and eol are pushed where necessary
   in order to perform the pretty printing of text in descan.

   Deparse calls itself recursively to deparse the subtrees in each node
   type that it encounters. The tokens are gathered in array tokenstream
   by the addtoken routine shared with scan.
*/

static void
deparse(nialptr br)
{
  nialptr     optn,
              arg,
              arg1,
              last,
              expr,
              x,
              body,
              blockbody,
              clist,
              elist,
              idlist,
              tr;
  nialint     i,
              cnt,
              ifi,
              n,
              in;
  int         limit;

  if (tally(br) == 0) {      /* deparse should never be given an empty tree */
    cleanuptokenstream();
    apush(makefault("?invalid parse tree in deparse"));
    return;
  }

  /* select deparse code based on the tag of the parse tree node */
  switch (tag(br)) {
    case t_constant:
        { /* pick up constant type from its value and set st */
          int         st = 0;
          nialptr     v = get_c_val(br);

          switch (kind(v)) {
            case inttype:
                st = sInteger;
                break;
            case realtype:
                st = sReal1;
                break;
            case phrasetype:
                st = sPhrase;
                break;
            case faulttype:
                st = sFault;
                break;
            case atype:      /* an empty string */
                st = sString1;
                break;
            case chartype:
                if (atomic(v))
                  st = sFChar;
                else
                  st = sString1;
                break;
            case booltype:
                st = sBool;
                break;
          }
          /* add the token with constant property */
          addtkn(createint(constprop + st));
          addtkn(get_c_pv(br));
          break;
        }

    case t_basic: 
        /* a basic name. get its identifer and role and map it to canonical form. */
        addtkn(createint(identprop));
        addtkn(mapident(bnames[get_index(br)], get_brole(br)));
        break;

    case t_expression:
    case t_variable:
        { /* a variable or expression. 
             get its identifer and role and map it to canonical form. */
          nialint     rol = get_role(br);
          addtkn(createint(identprop));
          addtkn(mapident(get_var_pv(br), rol));
          break;
        }

    case t_strand:
        /* deparse the items of the strand in order */
        for (i = 1; i < tally(br); i++)
          deparse(fetch_array(br, i));
        break;

    case t_opcall:
        /* deparse the operation followed by the argument */
        optn = get_op(br);
        arg = get_argexpr(br);
        deparse(optn);
        deparse(arg);
        break;

    case t_basic_binopcall:
        /* deparse the left argument then the operation 
           followed by the right argument */
        optn = get_op(br);
        arg = get_argexpr(br);
        arg1 = get_argexpr1(br);
        deparse(arg);
        deparse(optn);
        deparse(arg1);
        break;

    case t_curried:
        /* deparse the curried argument followed by the operation */
        optn = get_op(br);
        arg = get_argexpr(br);
        deparse(arg);
        deparse(optn);
        break;

    case t_defnseq:
    case t_exprseq:
        {  /* deparse each item separating them by ";"s */
          nialint     t = tally(br);     

         
          last = fetch_array(br, t - 1);
          for (i = 1; i < t - 1; i++) {
            deparse(fetch_array(br, i));
            emitrwd(r_SEMICOLON);
            emiteol();
          }
          if (last != Nulltree)
            deparse(last);
        }
        break;

    case t_assignexpr:
        /* deparse the idlist, insert the ":=" and deparse the right 
           hand side expression */
        idlist = get_idlist(br);
        deparse(idlist);
        emitrwd(r_ASSIGNSYM);
        expr = get_expr(br);
        deparse(expr);
        break;

    case t_indexedassign:
        /* deparse the index variable, insert the ":=" and deparse 
           the right hand side expression */
        deparse(get_ia_left(br));
        emitrwd(r_ASSIGNSYM);
        expr = get_ia_expr(br);
        deparse(expr);
        break;

    case t_pickplace:
        /* deparse the variable, insert the "@", and deparse the indexing expression */
        deparse(get_up_id(br));
        emitrwd(r_CAT);
        expr = get_up_expr(br);
        deparse(expr);
        break;

    case t_reachput:
        /* deparse the variable, insert the "@@", and deparse the indexing expression */
        deparse(get_up_id(br));
        emitrwd(r_CAT);
        emitrwd(r_CAT);
        expr = get_up_expr(br);
        deparse(expr);
        break;

    case t_slice: 
        /* deparse the variable, insert the "|", and deparse the indexing expression */
        deparse(get_up_id(br));
        emitrwd(r_BAR);
        expr = get_up_expr(br);
        deparse(expr);
        break;

    case t_choose:
        /* deparse the variable, insert the "#", and deparse the indexing expression */
        deparse(get_up_id(br));
        emitrwd(r_HASH);
        expr = get_up_expr(br);
        deparse(expr);
        break;

    case t_ifexpr:
        cnt = tally(br);
        /* insert IF, deparse the test expression, insert THEN and
           deparse the then expression */
        emitrwd(r_IF);
        deparse(get_test(br, 1));
        emitrwd(r_THEN);
        emitincrin();
        emiteol();
        deparse(get_thenexpr(br, 1));

        /* loop while more than 2 items left */
        cnt = cnt - 2;
        ifi = 3;
        while (cnt > 2) {
          /* insert ELSEIF, deparse the test expression, insert THEN and
           deparse the then expression */
          popeol();
          emitdecrin();
          emiteol();
          emitrwd(r_ELSEIF);
          deparse(get_test(br, ifi));
          emitrwd(r_THEN);
          emitincrin();
          emiteol();
          deparse(get_thenexpr(br, ifi));
          cnt = cnt - 2;
          ifi = ifi + 2;
        }
         
        if (cnt == 2) { /* else expression exists. Insert ELSE and
                           deparse the else expression */
          popeol();
          emitdecrin();
          emiteol();
          emitrwd(r_ELSE);
          emitincrin();
          emiteol();
          deparse(get_elseexpr(br, ifi));
        }

        /* insert the ENDIF */
        popeol();
        emitdecrin();
        emiteol();
        emitrwd(r_ENDIF);
        break;

    case t_whileexpr:
        /* insert WHILE, deparse the test expression, insert Do,
           deparse the expression sequence and insert ENDWHILE */
        emitrwd(r_WHILE);
        deparse(get_wtest(br));
        emitrwd(r_DO);
        emitincrin();
        emiteol();
        deparse(get_wexprseq(br));
        popeol();
        emitdecrin();
        emiteol();
        emitrwd(r_ENDWHILE);
        break;

    case t_exit:
        /* insert EXIT and deparse the expression */
        emitrwd(r_EXIT);
        deparse(get_eexprseq(br));
        break;

    case t_repeatexpr:
        /* insert REPEAT, deparse the expression sequence, insert UNTIL,
           deparse the test expression and insert ENDREPEAT */
        emitrwd(r_REPEAT);
        emitincrin();
        emiteol();
        deparse(get_rexprseq(br));
        popeol();
        emitdecrin();
        emiteol();
        emitrwd(r_UNTIL);
        deparse(get_rtest(br));
        emiteol();
        emitrwd(r_ENDREPEAT);
        break;

    case t_forexpr:
        /* insert FOR, deparse the for variable, insert WITH,
           deparse the expression sequence and insert ENDFOR */
        emitrwd(r_FOR);
        deparse(get_idlist(br));
        emitrwd(r_WITH);
        deparse(get_expr(br));
        emitrwd(r_DO);
        emitincrin();
        emiteol();
        deparse(get_fexprseq(br));
        popeol();
        emitdecrin();
        emiteol();
        emitrwd(r_ENDFOR);
        break;

    case t_caseexpr:
        /* insert CASE, deparse the selector expression, and insert FROM */
           
        emitrwd(r_CASE);
        deparse(get_ctest(br));
        emitrwd(r_FROM);
        emitincrin();
        emitincrin();
        emiteol();
        clist = get_sexprs(br);
        elist = get_eseqs(br);
        n = tally(clist);
        x = fetch_array(elist, n);
        if (x == Nulltree)
          limit = false;
        else
          limit = true;
        /* loop over the cases deparsing the constant, inserting the ":",
           deparsing the expression sequence and inserting END */
        for (in = 0; in < n; in++) {
          deparse(fetchasarray(clist, in));
          emitrwd(r_COLON);
          emitincrin();
          emiteol();
          deparse(fetch_array(elist, in));
          if (intval(fetch_array(tknstream, tokencnt - 2)) != eolprop)
            emiteol();
          emitrwd(r_END);
          emitdecrin();
          emiteol();
        }
        if (limit) { /* there is an else clause: insert ELSE and deparse
                        the expression sequence */
          emitrwd(r_ELSE);
          emitincrin();
          emiteol();
          deparse(x);
          popeol();
          emitdecrin();
        }

        /* insert the ENDCASE */
        popeol();
        emitdecrin();
        emitdecrin();
        emiteol();
        emitrwd(r_ENDCASE);
        break;

    case t_opform:
        /* insert OPERATION and deparse the argument list */
        body = get_body(br);
        emitrwd(r_OPERATION);
        deparse(get_arglist(br));
        if (tag(body) != t_blockbody) {
          if (tag(body) == t_parendobj) { /* body is in ( ) */
            /* insert "(", deparse body and insert ")" */
            emitrwd(r_LPAREN);
            emitincrin();
            emiteol();
            deparse(get_obj(body));
            emitrwd(r_RPAREN);
            emitdecrin();
          }
#ifdef SYNTACTIC_DOT
          else if (tag(body) == t_dottedobj) { /* insert "." and deparse body */
            emitrwd(r_DOT);
            deparse(get_obj(body));
          }
          else { /* body is a name */
            deparse(body);
          }
#endif
        }
        else { /* body is a block */
          blockbody = body;
          goto joinblockstuff;
        }
        break;

    case t_block:
        blockbody = get_bdy(br);

    joinblockstuff:
        /* insert "{" */
        emitrwd(r_LCURLY);
        emitincrin();
        emiteol();

        if (get_locallist(blockbody) != Null) { 
          /* insert LOCAL, deparse local variables and insert ";" */
          emitrwd(r_LOCAL);
          deparse(get_locallist(blockbody));
          emitrwd(r_SEMICOLON);
          emiteol();
        }

        /* nonlocallist is a list of variables. It has tally > 1 if there are
           any declared explicitly. */
        if (tally(get_nonlocallist(blockbody)) > 1) {
          /* insert NONLOCAL, deparse nonlocal variables and insert ";" */
          emitrwd(r_NONLOCAL);
          deparse(get_nonlocallist(blockbody));
          emitrwd(r_SEMICOLON);
          emiteol();
        }

        x = get_defs(blockbody);
        if (x != grounded) { /* has definitions, deparse them and insert ";" */
          deparse(x);
          emitrwd(r_SEMICOLON);
          emiteol();
        }

        /* deparse the expression sequence and insert "}" */
        deparse(get_seq(blockbody));
        emitrwd(r_RCURLY);
        emitdecrin();
        break;

    case t_idlist:
        /* loop to deparse each identifier */
        for (i = 1; i < tally(br); i++)
          deparse(fetch_array(br, i));
        break;

    case t_definition:
        /* deparse definition name, insert IS and deparse the body */
        arg = get_dname(br);
        deparse(arg);
        emitrwd(r_IS);
        body = get_dvalue(br);
        deparse(body);
        break;

    case t_ext_declaration:
        /* deparse the name, insert IS EXTERNAL , insert the role based on the body */
        arg = get_dname(br);
        deparse(arg);
        emitrwd(r_IS);
        emitrwd(r_EXTERNAL);
        body = get_dvalue(br);
        if (body == no_excode)
          emitrwd(r_EXPRESSION);
        else if (body == no_opcode)
          emitrwd(r_OPERATION);
        else if (body == no_trcode)
          emitrwd(r_TRANSFORMER);
        else if (body == no_value)
          emitrwd(r_VARIABLE);
        break;

    case t_composition:
        /* loop to deparse the items in the composition */
        for (i = 1; i < tally(br); i++)
          deparse(fetch_array(br, i));
        break;

    case t_list:
    case t_atlas:
    case t_galaxy:
        /* insert the "[" */
        /* loop to deparse the expressions and insert "," (except on the last one) */
        emitrwd(r_LBRACKET);
        for (i = 1; i < tally(br); i++) {
          expr = fetch_array(br, i);
          deparse(expr);
          if (i != (tally(br) - 1))
            emitrwd(r_COMMA);
        }
        /* insert the "]" */
        emitrwd(r_RBRACKET);
        break;

    case t_transform:
        /* deparse the transformer expression followed by the Operation expression */
        tr = get_tr(br);
        deparse(tr);
        deparse(get_argop(br));
        break;

    case t_trform:
        /* insert TRANSFORMER, deparse the arglist */
        emitrwd(r_TRANSFORMER);
        deparse(get_opargs(br));
        
        if (tag(get_trbody(br)) == t_opform) {
          /* if body is an opform start a new indeted line and deparse it */
          emitincrin();
          emiteol();
          deparse(get_trbody(br));
          emitdecrin();
        }
        else /* deparse the body on the same line */
          deparse(get_trbody(br));
        break;

    case t_commentexpr:
        /* add the comment to the stream */
        addtkn(createint(commentprop));
        addtkn(get_com(br));
        break;

    case t_parsetree:
        /* insert "!" and deparse the parse tree */
        emitrwd(r_PGMQUOTE);
        arg = fetch_array(br, 1);
        deparse(arg);
        break;

    case t_parendobj:
        /* insert "(", deparse the object and insert the ")" */
        emitrwd(r_LPAREN);
        deparse(get_obj(br));
        emitrwd(r_RPAREN);
        break;

#ifdef SYNTACTIC_DOT
    case t_dottedobj:
        /* insert the "." and deparse the obj */
        emitrwd(r_DOT);
        deparse(get_obj(br));
        break;
#endif

    case t_nulltree:  /* nothing to emit */
        break;

#ifdef DEBUG
    default:
        nprintf(OF_DEBUG, "bad type= %d in deparse case\n", tag(br));
#endif
  }
}

/* Nial primitive descan */

void
idescan()
{
  nialptr     x;

  if (kind(top) == faulttype) { /* arg is a fault */
    idisplay();              /* to make canonical work on faults */
    isolitary();
    return;
  }

  x = apop();
  if (tally(x) == 0) { /* arg is empty, an error */
    apush(makefault("?empty arg in descan"));
  }

  else /* check for an empty token stream */
  if (tally(x) == 1 && kind(x) == inttype && fetch_int(x, 0) == t_tokenstream) {
    apush(Null);
  }

  else /* check that arg is correct type */
  if (kind(x) != atype) {
    apush(makefault("?invalid type in descan"));
  }

  else /* check that arg has token stream tag */
  if (tag(x) == t_tokenstream)
    descan(x);

  else
    apush(makefault("?descan needs token stream as argument"));
  freeup(x);
}


/* Nial primitive deparse */

void
ideparse()
{
  nialptr     x;

  x = apop();
  if (kind(x) == faulttype) { /* arg is a fault, pass it through */
    apush(x);
  }

  else if (tally(x) == 0) { /* arg is empty, an error */
    apush(makefault("?empty arg in deparse"));
  }

  else { /* check that arg has a parse tree tag */
    if (kind(x) == atype && ptag(x) == t_parsetree) {
      /* initialize the token stream and deparse the parse tree */
      cleartokenstream();
      addtkn(createint(t_tokenstream));
      deparse(pbody(x));
      apush(finishtokenstream()); /* complete the token stresm and push it */
    }

    else
      apush(makefault("?deparse needs parse tree as argument"));
  }
  freeup(x);
}
