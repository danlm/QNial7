/*=============================================================

MODULE  LIB_MAIN.C

  COPYRIGHT NIAL Systems Limited  1983-2016

Does the initialization of the Q'Nial abstract machine and calls
initialization routines for many aspects of the interpreter.

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

/* SIGLIB */
#include <signal.h>

/* SJLIB */
#include <setjmp.h>

/* MATHLIB */
#include <math.h>


/* Q'Nial header files */

#include "qniallim.h"        /* must go before lib_main.h */
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"
#include "if.h"

#include "version.h"

#include "fileio.h"          /* for nprintf etc. */
#include "roles.h"           /* role codes */
#include "symtab.h"          /* symbol table macros */
#include "blders.h"          /* for b_basic in initialization */
#include "wsmanage.h"        /* for wsload */
#include "parse.h"           /* parse tree node tags */
#include "lexical.h"         /* for BLANK */
#include "resnms.h"



/* prototypes for local routines */

extern void sysinit(void);
extern void signon(void);
extern void exit_cover1(char *errmsg, int errorlevel);



static void init_res_words(void);
static void do_init(char *name, nialptr * globevar);
static void init_fncodes(void);
static void banner(char *b);

/* Declare the global struct defined in lib_main.c that holds global
   variables shared between modules.
*/

struct global_vars1 G1;


/* Declare the global struct that contains values that are saved in a 
   workspace dump. A struct is used so that only a single read and write
   is needed to load or save the information. 
*/

struct global_vars G;

static nialint nmcnt,
            bincnt;          /* globals used to assign indices to basic names
                                across multiple calls to init_bnames. */

static char qcr[] =
"Copyright (c) NIAL Systems Limited";



/* routine to implement Restart primitive expression */

void
irestart(void)
{ 
    apush(makefault("not implemented\n"));
}



/* routine to construct the banner */

static void
banner(char *b)
{
  strcpy(b, "Q'Nial V7.0");
  strcat(b, " Open Source Edition");
  strcat(b, " Intel x86");
  strcat(b, wordsize);
  strcat(b, opsys);
  strcat(b, debugstatus);
  strcat(b, " ");
  strcat(b, __DATE__);
}

/* routine to write the banner and copyright */

void
signon()
{
  if (!quiet) {
    char        versionbuf[1000];

    banner(versionbuf);
    writechars(STDOUT, versionbuf, (nialint) strlen(versionbuf), true);
    writechars(STDOUT, qcr, (nialint) strlen(qcr), true);
  }
}


/* routines to create primitive routines that return faults for 
   undefined expressions, operations and trs */

void
ino_expr(void)
{
  buildfault("missing_expr");
}

void
ino_op(void)
{
  freeup(apop());
  buildfault("missing_op");
}

void
ino_tr(void)
{
  freeup(apop());
  apop();
  buildfault("missing_tr");
}

/* primitive constant expressions */

void
inull(void)
{
  apush(Null);
}

void
itrue(void)
{
  apush(True_val);
}

void
ifalse(void)
{
  apush(False_val);
}

/* routine sysinit initializes Nial variables for the initial workspace. They are 
   stored in the struct G and saved in a workspace dump.
*/

void
sysinit()
{
  nialptr     n;
  int         i;
  nialint     zero = 0,
              one = 1;
  nialint     dummy;         /* used as a ptr to an empty list of ints */

  topstack = -1;

#ifdef DEBUG
  memchk();
#endif

  /* set up initial arrays */

/* put in initial ints. Used to avoid constructing frequent small values */
  for (i = 0; i < NOINTS; i++) {
    n = new_create_array(inttype, 0, 1, &dummy);
    store_int(n, 0, i);
    intvals[i] = n;
    incrrefcnt(n);
  }

  Zero = intvals[0];
  One = intvals[1];
  Two = intvals[2];

  Blank = createchar(BLANK);
  incrrefcnt(Blank);

/* Null made here, so it can be used in maketkn for initial properties
   of phrases and faults */

  Null = new_create_array(atype, 1, 0, &zero);
  incrrefcnt(Null);

  grounded = createatom(faulttype, "?Grnd");
  incrrefcnt(grounded);

  True_val = new_create_array(booltype, 0, 1, &dummy);
  store_bool(True_val, 0, true);
  incrrefcnt(True_val);

  False_val = new_create_array(booltype, 0, 1, &dummy);
  store_bool(False_val, 0, false);
  incrrefcnt(False_val);

  Voidstr = new_create_array(chartype, 1, 0, &one);
  store_char(Voidstr, 0, '\0');
  incrrefcnt(Voidstr);

  Nullexpr = createatom(faulttype, "?noexpr");
  incrrefcnt(Nullexpr);

  r_EOL = createatom(phrasetype, "E O L");
  incrrefcnt(r_EOL);

  Nulltree = b_nulltree();
  incrrefcnt(Nulltree);

  { double      r = 0.0;
    Zeror = new_create_array(realtype, 0, 1, &dummy);
    store_real(Zeror, 0, r);
    incrrefcnt(Zeror);
  }

  Typicalphrase = makephrase("\"");
  incrrefcnt(Typicalphrase);

  Eoffault = createatom(faulttype, "?eof");
  incrrefcnt(Eoffault);

  Zenith = createatom(faulttype, "?I");
  incrrefcnt(Zenith);

  Nadir = createatom(faulttype, "?O");
  incrrefcnt(Nadir);

  no_value = createatom(faulttype, "?no_value");
  incrrefcnt(no_value);

  Logical = createatom(faulttype, "?L");
  incrrefcnt(Logical);


#ifdef DEBUG
  memchk();
#endif

  /* set up the global symbol table */

  global_symtab = addsymtab(stpglobal, "GLOBAL");
  incrrefcnt(global_symtab);
  current_env = Null;

  /* Debug lists initialized */
  breaklist = Null;
  incrrefcnt(breaklist);
  watchlist = Null;
  incrrefcnt(watchlist);



  /* initialize the list of reserved words */
  init_res_words();

  nmcnt = 0;
  bincnt = 0;

/* call the routine generated in basics.c that initializes the
   primitives tables.
*/

  initprims();     /* defined in the generated file basics.c */

  init_fncodes();            /* initializes parse tree nodes
                                for internally referenced Nial objects */

#ifdef DEBUG
  memchk();
#endif

}




/* routine to set up reserved words in initial ws.
   The info is in table reswords, which is generated from
   reserved.h. The latter gives a defined name
   to each reserved word as strings in upper case.
   The defined names are used in the parser so that changes
   to the reserved words can be made easily.
*/

static void
init_res_words()
{
  int         i;

  for (i = 0; i < NORESWORDS; i++)
    mkSymtabEntry(global_symtab, makephrase(reswords[i]), Rres, no_value, true);
}

/* routine to initialize a C variable to a parse tree node */

static void
do_init(char *name, nialptr * globevar)
{
  nialptr     tkn,
              sym,
              entr;

  tkn = makephrase(name);
  entr = lookup(tkn, &sym, passive);
  if (entr == notfound) {
    nprintf(OF_MESSAGE, "Error in init_fncodes\n");
    nprintf(OF_NORMAL_LOG, name);
  }
  else {
    *globevar = sym_valu(entr);
    incrrefcnt(*globevar);
  }
}

/* looks up function table pointers and stores them in global variables
for the evaluator. Done after initializing basics so that it can
link an internal code with a nial basic expression, operation or
transformer. The internal code is used to access the definition
using a parse tree rather than a C pointer, allowing them to be
fed to the nial evaluation mechanism.

In the SHELL versions of the interpreter the symbols like + and *
are replaced by names like SUM and PRODUCT.
*/

static void
init_fncodes()
{
#if defined(V4SHELL) || defined(V6SHELL)
  do_init("NO_EXPR", &no_excode);
  do_init("NO_OP", &no_opcode);
  do_init("NO_TR", &no_trcode);
  do_init("_LTE", &ltecode);
  do_init("_GTE", &gtecode);
  do_init("_LEAF", &leafcode);
  do_init("_TWIG", &twigcode);
  do_init("_SUM", &sumcode);
  do_init("_PRODUCT", &productcode);
  do_init("_MIN", &mincode);
  do_init("_MAX", &maxcode);
  do_init("_AND", &andcode);
  do_init("_OR", &orcode);
  do_init("_LINK", &linkcode);
  do_init("_PLUS", &pluscode);
  do_init("_TIMES", &timescode);
  do_init("_FIRST", &firstcode);
  do_init("_SINGLE", &singlecode);
  do_init("_CONVERSE", &conversecode);
  do_init("_UP", &upcode);
  do_init("_EQUAL", &equalcode);
  do_init("_EACHRIGHT", &eachrightcode);
#else
  do_init("NO_EXPR", &no_excode);
  do_init("NO_OP", &no_opcode);
  do_init("NO_TR", &no_trcode);
  do_init("<=", &ltecode);
  do_init(">=", &gtecode);
  do_init("LEAF", &leafcode);
  do_init("TWIG", &twigcode);
  do_init("+", &sumcode);
  do_init("*", &productcode);
  do_init("MIN", &mincode);
  do_init("MAX", &maxcode);
  do_init("AND", &andcode);
  do_init("OR", &orcode);
  do_init("LINK", &linkcode);
  do_init("PLUS", &pluscode);
  do_init("TIMES", &timescode);
  do_init("FIRST", &firstcode);
  do_init("SINGLE", &singlecode);
  do_init("CONVERSE", &conversecode);
  do_init("UP", &upcode);
  do_init("=", &equalcode);
  do_init("EACHRIGHT", &eachrightcode);
#endif
}

/* routine to install the Nial name corresponding to the internal C
routine for a primitive.  Called in init_prims() */

void
init_primname(char *opname, char prop)
{
  nialptr     id,
              tree,
              role = -1;

  switch (prop) {
    case 'B':                /* binary not pervasive */
    case 'C':                /* binary pervasive */
    case 'P':                /* unary pervasive */
    case 'R':                /* multi pervasive */
    case 'U':                /* unary not pervasive */
        role = Roptn;
        break;
    case 'E':
        role = Rexpr;
        break;
    case 'T':
        role = Rtrans;
        break;
    default:
        nprintf(OF_MESSAGE, "invalid entry in init_primname\n");
  }
  id = makephrase(opname);
  tree = b_basic(nmcnt, role, prop, bincnt);
  mkSymtabEntry(global_symtab, id, role, tree, true);
  bnames[nmcnt] = id;
  incrrefcnt(id);
  nmcnt++;                   /* counter for unary routines */
  if (prop == 'B' || prop == 'C' || prop == 'R')
    bincnt++;                /* these operation classes also have a binary
                              * routine */
}



/* exit_cover1 is the routine that handles all jumps out of the normal
 flow of the evaluator. There are several circumstances where this
 can happen, such as:
 fatal errors
 termination request by program control
 user interrupt (Ctrl C or equivalent)
 program interrupt (toplevel call)
 internal warnings (to be recovered from)
 
 The routine prints the supplied errmsg string and longjumps with the errorlevel.
 Each return code is assigned to a class.
 */



void
exit_cover1(char *errmsg, int errorlevel)
{  if (!quiet)
     printf("%s\n",errmsg);
   longjmp(error_env,errorlevel);
}



