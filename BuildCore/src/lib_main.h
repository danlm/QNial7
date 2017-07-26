/*==============================================================

  HEADER LIB_MAIN.H

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the structures for global values that are saved with workspaces
  and the macros that access the values.

================================================================*/

#ifndef _LIB_MAIN_H_
#define _LIB_MAIN_H_

extern int do_init_work(void);  /* usind in main_stu.c */
extern void exit_cover(int flag);
extern void exit_cover1(char *errmsg, int errorlevel);

extern void cleanup_ws(void);




/*   global variable definitions    */

 /* The following global struct contains values that are shared between modules. 
    This file also holds the defines that renames each field so it can be 
    referred to without the G1.g_ prefix. */


struct global_vars1 {
  int         g_instartup;   /* variable set in lib_main.c to indicate
                                whether defined names created by parse are
                                system or user names */
  int         g_doingsysinit; /* needed for absmach.c to create Null during startup */
  int         g_quiet;       /* indicates suppression of default output */
  int         g_expansion;   /* indicates that heap can expand */
  int         g_nouserinterrupts; /* flag set by user to inhibit interrupts */
  int         g_userbreakrequested; /* global used for communication of break request */

  int         g_EOFsignalled; /* for CTRL C stuff */

  nialint     g_initmemsize; /* initial memory size saved for a restart */
  jmp_buf     g_init_buf;    /* buffer for long jumps during startup */
  char        g_gcharbuf[GENBUFFERSIZE];  /* generic buffer to save space */
  int         g_keeplog;     /* on if log is being kept */
  int         g_doinglatent;/* signals that we are doing a latent execution */
  nialint     g_ssizew;      /* effective screen width */
  nialptr     g__x_;   /* used as temporary in alternate apush and apop macros */
  char        g_logfnm[MAXLOGFNMSIZE + 1];  /* holds the log file name */
  char        g_nprompt[MAXPROMPTSIZE + 1];  /* holds the prompt */
  char       *g_errmsgptr;   /* used to hold error messages */
  int         g_invseq[HIGHCHAR - LOWCHAR + 1]; /* inverted collating sequence */
#ifdef DEBUG
  int         g_debug;       /* turns on debugging code */
#endif
/* heap variables */
  nialword    *g_mem;         /* pointer to the heap area */
  nialint     g_memsize;     /* size of heap in words */
  nialptr     g_membase;     /* first useable heap block in memory */
  nialptr     g_freelisthdr; /* offset to fake block that points to the free list */
/* C buffer variables */
  char       *g_Cbuffer;     /* Cbuffer */
  char       *g_ptrCbuffer;  /* next available position in the Cbuffer */
  char       *g_startCbuffer;/* start of Cbuffer work area */
  char       *g_finCbuffer;  /* end of Cbuffer work area */
/* stack variables */
  nialptr    *g_stkarea;     /* pointer to Nial stack work area */
  nialint     g_stklim;      /* maximum size of current Nial stack */
  nialint     g_topstack;    /* offset to top of stack. (-1) when stack is empty */
  int         g_triggered;   /* flag used to indicate whether faults should
                              * trigger. Default is yes! */
  int         g_nialexitflag;/* used to control exit from exprseqs */
  int         g_trace;       /* on if tracing is being done */
  int         g_debugging_on;/* debugging active switch */
  jmp_buf     g_error_env;   /* for error conditions and callbacks */
  FILE       *g_wsfileport;  /* holds file pointer for wsload or wsdump */
/* from symtab */
  nialptr     g_nonlocs;     /* list of nonlocals shared by parse and symtab */
  nialptr     g_current_env; /* the list of symbol tables in the current env */
  nialint     g_atomtblsize; /* size of area for phrases and faults */
  nialptr    *g_atomtbl;     /* atom table area for phrases and faults */
#ifdef PROFILE
  int         g_profile;     /* global variable that indicates profiling is
                                occurring. visible to eval.c and trs.c */
#endif
};

extern struct global_vars1 G1;


 /* This global struct contains values that are saved in a workspace dump. A
    struct is used to allow a single read and write to be used. This file
    also holds the defines that renames each field so it can be referred to
    without the G.g_ prefix. */

struct global_vars {
/* atom table variables */
  nialptr     g_atomtblbase; /* atomtblbase when ws is saved */
  nialint     g_atomcnt;     /* number of atoms in use */
  nialptr     g_stkareabase; /* stkarea when ws is saved */
  nialint     g_wssize;      /* wssize when ws is saved */
  nialptr     g_firstfree;   /* link to first free in saved workspace */
  nialptr     g_intvals[NOINTS];  /* holds low Nial integers */
  nialptr     g_bnames[NOBNAMES]; /* the names for built-in objects */
  nialptr     g_filenames;   /* the names for open files */

  /* Debugging lists (watch and break) */
  nialptr     g_watchlist,
              g_breaklist;


  /* Nial constants */
  nialptr     g_Zero,
              g_One,
              g_Two,
              g_True,
              g_False,
              g_Null,
              g_Voidstr,
              g_Nullexpr,
              g_Zeror,
              g_Blank,
              g_Typicalphrase,
              g_Nulltree,
              g_Eoffault,
              g_Zenith,
              g_Nadir,
              g_grounded,    /* used to initialize symboltable entries */
              g_no_value,    /* no value fault */
              g_Logical,    /* Logical fault */
              g_r_EOL,       /* EOL cosntant used in parse */
              g_global_symtab;  /* the Nial structure holding the global
                                   symbol table as a binary tree */
  int         g_sketch,
              g_decor,
              g_messages_on
#ifdef DEBUG
             ,
              g_debug_messages_on
#endif
             ;

  /* variables for operator and operation codes */
  nialptr     g_no_excode,
              g_no_opcode,
              g_no_trcode,
              g_sumcode,
              g_productcode,
              g_maxcode,
              g_mincode,
              g_andcode,
              g_orcode,
              g_linkcode,
              g_pluscode,
              g_timescode,
              g_firstcode,
              g_singlecode,
              g_leafcode,
              g_conversecode,
              g_twigcode,
              g_ltecode,
              g_gtecode,
              g_upcode,
              g_equalcode,
              g_eachrightcode;
};

extern struct global_vars G;

/* defines to access global structure as variables */

/* extern int userbreakrequested;*/
extern void sysinit(void);
extern void signon(void);

#define instartup G1.g_instartup
#define doingsysinit G1.g_doingsysinit
#define quiet G1.g_quiet
#define expansion G1.g_expansion
#define doinglatent G1.g_doinglatent
#define nouserinterrupts G1.g_nouserinterrupts
#define userbreakrequested G1.g_userbreakrequested
#define EOFsignalled G1.g_EOFsignalled
#define loadfnm G1.g_loadfnm
#define initmemsize G1.g_initmemsize
#define init_buf G1.g_init_buf
#define gcharbuf G1.g_gcharbuf
#define keeplog G1.g_keeplog
#define ssizew G1.g_ssizew
#define _x_ G1.g__x_
#define logfnm G1.g_logfnm
#define nprompt G1.g_nprompt
#define errmsgptr G1.g_errmsgptr
#define invseq G1.g_invseq
#ifdef DEBUG
#define debug G1.g_debug
#endif
/* from absmach */
#define mem G1.g_mem
#define memsize G1.g_memsize
#define membase G1.g_membase
#define freelisthdr G1.g_freelisthdr
#define Cbuffer G1.g_Cbuffer
#define ptrCbuffer G1.g_ptrCbuffer
#define startCbuffer G1.g_startCbuffer
#define finCbuffer G1.g_finCbuffer
#define stkarea G1.g_stkarea
#define stklim G1.g_stklim
#define topstack G1.g_topstack
/* from eval */
#define triggered G1.g_triggered
#define nialexitflag G1.g_nialexitflag
#define trace G1.g_trace
#define debugging_on G1.g_debugging_on
/* from main_stu */
#define error_env G1.g_error_env
/* from mainlp */
#define wsfileport G1.g_wsfileport
#define nonlocs G1.g_nonlocs
#define current_env G1.g_current_env
#define profile G1.g_profile
#define atomtblsize G1.g_atomtblsize
#define atomtbl G1.g_atomtbl

#define watchlist G.g_watchlist
#define breaklist G.g_breaklist

#define savedmemsize G.g_savedmemsize
#define atomtblbase G.g_atomtblbase
#define atomcnt G.g_atomcnt
#define stkareabase G.g_stkareabase
#define wssize G.g_wssize
#define firstfree G.g_firstfree
#define intvals G.g_intvals
#define bnames G.g_bnames
#define filenames G.g_filenames
#define  Null G.g_Null
#define  Nullexpr G.g_Nullexpr
#define  Nulltree G.g_Nulltree
#define  no_value G.g_no_value
#define  Logical G.g_Logical
#define  r_EOL G.g_r_EOL
#define  grounded G.g_grounded
#define decor G.g_decor
#define sketch G.g_sketch
#define	messages_on	G.g_messages_on
#ifdef DEBUG
#define	debug_messages_on	G.g_debug_messages_on
#endif

#define  global_symtab G.g_global_symtab

#define Zero G.g_Zero
#define  One G.g_One
#define  Two G.g_Two
#define True_val G.g_True
#define  False_val G.g_False
#define  Voidstr G.g_Voidstr
#define Zeror G.g_Zeror
#define  Blank G.g_Blank
#define  Zeroc G.g_Zeroc
#define  Typicalphrase G.g_Typicalphrase
#define  Eoffault G.g_Eoffault
#define  Zenith G.g_Zenith
#define  Nadir G.g_Nadir

/* defines which access global structure as variables */

#define no_excode G.g_no_excode
#define no_opcode G.g_no_opcode
#define no_trcode G.g_no_trcode
#define leafcode G.g_leafcode
#define conversecode G.g_conversecode
#define firstcode G.g_firstcode
#define singlecode G.g_singlecode
#define twigcode G.g_twigcode
#define ltecode G.g_ltecode
#define gtecode G.g_gtecode
#define sumcode G.g_sumcode
#define productcode G.g_productcode
#define maxcode G.g_maxcode
#define mincode G.g_mincode
#define andcode G.g_andcode
#define orcode G.g_orcode
#define linkcode G.g_linkcode
#define pluscode G.g_pluscode
#define timescode G.g_timescode
#define upcode G.g_upcode
#define equalcode G.g_equalcode
#define eachrightcode G.g_eachrightcode


#define false 0
#define true 1

/* type of checksignal calls */

enum {
    NC_CS_STARTUP, NC_CS_NORMAL, NC_CS_OUTPUT, NC_CS_INPUT
};

/* types of messages */

enum {
    NC_FATAL = 1, NC_WARNING, NC_FAULT, NC_ABORT, NC_BYE,
    NC_WS_LOAD, NC_WS_SAVE, NC_BAD_ERROR_TYPE
};


#define sysdefsfnm "defs.ndf"

#ifdef DEBUG
#include "diag.h"
#endif


#endif
