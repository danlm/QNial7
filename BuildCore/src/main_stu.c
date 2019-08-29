/*============================================================

  MODULE   MAIN_STU.C

  COPYRIGHT NIAL Systems Limited  1983-2016

Contains the code specific to a console version of Q'Nial.

================================================================*/

#include "switches.h"



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

#ifdef UNIXSYS
/* Linenoise terminal initialisation */
#include "linenoise.h"
#define MAX_HISTORY_LEN 4096
int linenoiseMultiLineMode = 0;
#endif

#include "qniallim.h"
#include "lib_main.h"
#include "if.h"
#include "fileio.h"          /* for closeuserfiles and nprintf */
#include "eval.h"            /* for destroy_call_stack & varname */
#include "absmach.h"         /* for decrrefcnt and freeup */
#include "basics.h"          /* for isetformat() */
#include "picture.h"         /* for stdformat */
#include "wsmanage.h"        /* for loaddefs & wsload */

#include "lexical.h"         /* for BLANK and HASHSYMBOL */
#include "symtab.h"          /* symbol table macros */
#include "roles.h"           /* role codes */
#include "parse.h"           /* for parse */
#include "profile.h"         /* for clear_profiler */
#include "systemops.h"       /* for ihost */
#include "blders.h"
#include "token.h"


/* local globals */



static char inputline[INPUTSIZELIMIT];
static nialint lastval;




/* prototypes for local routines */

static void print_syntax(void);
static nialint get_memsize(char *mstr);
static void set_cseq(char *cseq);
extern void cleanup_ws(void);
static int latenttest(void);


/* These are needed for CMDLINE which is in userops.c  */


char      **global_argv = NULL;
int         global_argc;


/* Hold the initial Nial environment. Used by sprocess.c and others */

char **nial_envp;
int  nial_argc;
char **nial_argv;


/* The main program for the QNial interpreter.
 It does the following:
   - sets initial values for global variables
   - processes the command line input
   - initializes the interpreter
   - completes initialization
   - enters the evaluation loop that handles all top level user input
        - these can be definitions or epxressions to be pprocessed
        - they can also be a )save request, a Unix command, store last value request
        - loop is left by a Bye request or an error condition
   - cleans up on exit
 */
int
main(int argc, char *memin[], char **envp)
{
  int         i,
              defssw = false,
              latentfound = false,
              loadsw = false,
              recoverycnt = 0,
              nomainloop,
              error_rc;

  char        indefsfnm[MAX_FILENM_SIZE],
              inwsfnm[MAX_FILENM_SIZE];
    
  /* Save the initial environment */
  nial_envp = envp;
  nial_argc = argc;
  nial_argv = memin;

  /* set global variables */

    
  ssizew = 80;      /* default size of output width */
  initmemsize = dfmemsize;
  expansion = true;
  sketch = true;
  decor = false;
  strcpy(logfnm,"auto.nlg");
  strcpy(stdformat,"%g");
  strcpy(nprompt,"     ");
  latentfound = false;
  nialexitflag = false;
    
  /* switch settings if -i is not indicated in command line */
    
  quiet = true;  
  debugging_on = false;
  triggered = false;
  nomainloop = true;
  nouserinterrupts = true;
  keeplog = false;
  messages_on = false;

#ifdef UNIXSYS
  /**
   * Initialise the linenoise history aspect
   */
  linenoiseHistorySetMaxLen(MAX_HISTORY_LEN);
  linenoiseSetMultiLine(linenoiseMultiLineMode);
#endif

  /* This is a variable used in absmach.c during debugging of the interpreter.
     Turn it on  for debugging with the printf statements protected by the test
     if (doprintf) */
    
  doprintf = false;
    

  lastval = -1;

#ifdef DEBUG
  debug = false;  /* turn on to display print statements in parse.c */
#endif

#ifdef PROFILE
  profile = false;
#endif

#ifdef CMDLINE
  /* Export the command line to the rest of the code */
    
  global_argv = memin;
  global_argc = argc;
#endif
    
  instartup = true;         /* startup process begins */

  /* set up error recovery during initialization */
    
  recoverycnt = 0;

  error_rc = setjmp(error_env);

  if (error_rc) {
      
    /* all exits during startup come here */
      recoverycnt += 1;
      if (recoverycnt > 1)
        exit_cover1("startup error recovery mexhanism looping",NC_FATAL);
      
      switch(error_rc) {
              /* restart after these interruupts */
          case NC_FAULT:
          case NC_WARNING:
          case NC_ABORT:
          case NC_FATAL:
              exit(error_rc);
              
          default:
              exit(error_rc);
      }
  }
    
    
  /* end of init error handling */


  /* process command arguments
     the allowed syntax is:
     [(+|-)size nnnn] [-defs defsfilenm] [-lws wsfilenm]
        [-i] [-h]
   */
    
  i = 1;
  while (i < argc) {
    if (strcmp(memin[i], "-size") == 0) {
      /* set iniital size and allow expansion */
      initmemsize = get_memsize(memin[i + 1]);
      if (initmemsize < minmemsize)
        initmemsize = minmemsize;
      expansion = true;
      i += 2;
    }
    else
    if (strcmp(memin[i], "+size") == 0) {
      /* set iniital size and disallow expansion */
      initmemsize = get_memsize(memin[i + 1]);
      if (initmemsize < minmemsize)
        initmemsize = minmemsize;
      expansion = false;
      i += 2;
    }
    else
    if (strcmp(memin[i], "-defs") == 0) {
      if (i < argc && memin[i+1] != NULL) {
        /* explicit defs file name given */
        defssw = true;
        strcpy(indefsfnm, memin[i + 1]);
        i += 2;
      }
      else
        exit_cover1("missing definition file name after -defs option", NC_FATAL);
    }
    else
    if (strcmp(memin[i], "-lws") == 0) {
        
      if (i < argc && memin[i+1] != NULL) {
        /* explicit ws file name given */
        loadsw = true;
        strcpy(inwsfnm, memin[i + 1]);
        i += 2;
      }
      else
        exit_cover1("missing workspace file name after -lws option", NC_FATAL);
    }
    else
    if (strlen(memin[i]) == 2 && strcmp(memin[i], "-i") == 0) {
      /* use interactive mode */
      quiet = false;
      messages_on = true;
      debugging_on = true;
      triggered = true;
      nomainloop = false;
      nouserinterrupts = false;
      keeplog = true;
      i++;
    }
        
    else
    if (strcmp(memin[i], "-h") == 0) {
      /* display the help syntax and exit */
      print_syntax();
      exit(1);
    }
    else {
      exit_cover1("invalid command line arg", NC_FATAL);
    }
  }
    
/* end of code to process arguments */

  /* If nothing to be done then just exit with a help message */
  if (nomainloop && !loadsw && !defssw) {
    print_syntax();
    exit(1);
  }
  
  
/* call initialization code for various features. These routines are
     setting global C data for later use. */
    
  set_cseq(collatingseq);  /* invert the collating sequence */
  initboxchars(false);     /* set characters used in diagram */
  initfpsignal();          /* initializes signal handler for floating point
                            exceptions. */
  initunixsignals();       /* initial other Unix signals */
  signal(SIGINT, controlCcatch);  /* initialize the user break capability */
  if (!nomainloop)
    signon();   /* print the version and copywright banners */
#ifdef TIMEHOOK
  inittime();                /* initialize time for session */
#endif
    
  setup_abstract_machine(initmemsize); /* set up the heap, stack and buffer areas */
    
  /* complete set up of workspace by loading one or creating initial one */
    
  if (loadsw) {
    char        wsstr[256];
    FILE       *wsport;  /* starting value */
    strcpy(wsstr, inwsfnm);
    check_ext(wsstr, ".nws",NOFORCE_EXTENSION); /* adds .nws if missing */
    wsport = openfile(wsstr, 'r', 'b');
    if (wsport == OPENFAILED) {
      exit_cover1("Failed to open workspace file",NC_WARNING);
  }
  wsload(wsport); /* load the workspace file */
  if (!quiet && !nomainloop)
    nprintf(OF_NORMAL_LOG, "starting with workspace %s\n", wsstr);
      if (latenttest()) {      /* Latent exists. */
          current_env = Null;        /* set to global environment */
          nialexitflag = false;
          latentfound = true;
          /* fall through to complete startup */
      }
      
#ifdef DEBUG
        memchk();
        if (debug)
          nprintf(OF_DEBUG, "initial workspace loaded -  memchk done\n");
#endif
      
  }
  else {   /* create the starting workspace */
    doingsysinit = true;    /* flag set to allow Null to be created */
    sysinit();              /* create the initial values in the workspace */
    doingsysinit = false;
    loaddefs(false, "", 0); /* silent loaddefs of internal defs.ndf structure */
    clearheap();            /* to remove anything left over */
    if (!quiet && !nomainloop)
      nprintf(OF_NORMAL_LOG, "clear workspace created\n");
  }
    
  /* startup process finished. */

  instartup = false;
  
  /* set up error recovery during execution */
  error_rc = setjmp(error_env);
    
  if (error_rc) {

      recoverycnt += 1;
      
      /* avoid looping in error recovery */
      
      if (recoverycnt > 1)
      { nprintf(OF_MESSAGE_LOG, "error recovery mechanism looping\n");
          exit(NC_FATAL);
      }
      
      /* latentfound has to be reset here in case the latent expression causes
         a jump to top level */
      
      if (latentfound)
          latentfound = false;
    switch(error_rc) {
            
         /* restart after these interrupts */
        case NC_FAULT:
        case NC_WARNING:
          cleanup_ws();
          recoverycnt -= 1;
          goto retry;
            
        /* exit with error on these interrupts */
        case NC_ABORT:
        case NC_FATAL:
            nprintf(OF_MESSAGE_LOG, "exit with error flag %d\n",error_rc);
            exit(1);
            
            /* exit normally on these interrupts */
        case NC_BYE:
            exit(0);
            
            /* handle ws save here */
        case NC_WS_SAVE:
            clearstack();
            wsdump(wsfileport);
            cleanup_ws();
            goto retry;
            
            /* handle ws load here */
        case NC_WS_LOAD:
            if (doinglatent) {
               nprintf(OF_MESSAGE_LOG, "load attempted in Latent. exiting\n");
               exit(NC_FATAL);
            }
            
            wsload(wsfileport);
            current_env = Null;      /* set to global environment */
            lastval = -1;
            /* clear out old file system when the workspace was saved */
            decrrefcnt(filenames);
            freeup(filenames);
            /* restart file system of loaded workspace */
            startfilesystem();
            /* set initial prompt */
            strcpy(nprompt, "     ");
            init_debug_flags();
            cleanup_ws();
            if (latenttest()) {      /* Latent exists. */
              current_env = Null;        /* set to global environment */
              nialexitflag = false;
              latentfound = true;
              /* we fall through to continue setup */
            }
            goto retry;
            

            
        default:
              nprintf(OF_MESSAGE_LOG, "exiting with default case in top level loop \n");
            exit(error_rc);
    }
  }

  startfilesystem();  /* initialize the user file system
                         Do it here in case the loaddefs needs the filesystem */

  /* load the definitions file specified in the command line if present */
    
    current_env = Null;  /* this must be set here for loaddefs to work */
    
  if (defssw) {
    char        defstr[256];
    strcpy(defstr, indefsfnm);
    check_ext(defstr, ".ndf",NOFORCE_EXTENSION); /* adds .ndf if missing */

    if (!loaddefs(true, defstr, 0))
        exit_cover1("Failed to load defs.ndf",NC_FATAL);

  }

  /* skip the main loop if not started with -i option */
  if (nomainloop && !latentfound)
      goto cleanup;

  /* the top level loop */
    
    
retry: /* start over point for the main loop */

  {

      /* These variables are set here rather than in ieval() because of
       the possibility of ieval calling itself recursively using execute. */
      
      current_env = Null;        /* set to global environment */
      nialexitflag = false;
   
      if (latentfound) {
          doinglatent = true;
          topstack = (-1);
          mkstring("Latent");
          goto commandreentrypoint;
      }
      
      topstack = (-1);   /* resets the stack */

    /* prompt to get inputline
       We use rl_gets so that the input history is available in the top level loop */
      
      rl_gets(nprompt, inputline);
      mkstring(inputline);
      checksignal (NC_CS_INPUT);
      if (keeplog) {
          writelog(nprompt, strlen(nprompt), false);
          writelog(inputline, strlen(inputline), true);
      }
#ifdef DEBUG
    { nialint  total;
      total = checkavailspace();
      nprintf(OF_DEBUG, "available space %d\n", total);
      checkfortemps();
      memchk();
    }
#endif
 
      /* interpret the input stored in a NIal strong  */

    if (tally(top) == 0) {
      freeup(apop());   /* to get rid of empty input line */
      goto retry;
    }
 
commandreentrypoint:


   {
      char        firstchar;
      int         i = 0;
      
      /* move past Blanks if found */
      while (i < tally(top) && fetch_char(top, i) <= BLANK)
        i++;

      /* examine first char for # ! ] */
       
      firstchar = fetch_char(top, i);
      if (firstchar == RIGHTBRACKET) {
        nialptr     z,
                    name,
                    sym,
                    entr,
                    idlist;
          
        iscan();  /* scan the line beginning with ']' into z */

        z = apop();
        if (lastval == -1) {      /* no last eval to assign */
          freeup(z);
          exit_cover1("no previous value to assign",NC_WARNING);
        }

        if (tally(z) >= 5) {       /* there is an argument to hist */
          if (intval(fetch_array(z, 3)) == identprop) {
            /* do assignment directly using assign */
            /* get the name and validate it */
            name = fetch_array(z, 4);
            if (varname(name, &sym, &entr, false, true)) {
              if (sym_role(entr) == Rvar || sym_role(entr) == Rident) {
                if (sym_role(entr) == Rident)
                  st_s_role(entr, Rvar);
                  
                /* build an idlist parse tree node and do the assignment */

                idlist = nial_extend(st_idlist(), b_variable((nialint) sym, (nialint) entr));
                assign(idlist, lastval, false, false);
                freeup(idlist);
                freeup(z);
                lastval = -1;
                goto retry;
              }
            }
          }
        }
        freeup(z);
        nprintf(OF_MESSAGE_LOG, "invalid syntax for getting last value\n");
        goto retry;  /* using exit_cover1 would lose current last value */
      }
       
      if (firstchar == HASHSYMBOL) {
        /* the line is a remark, just remove it */
        freeup(apop());
        goto retry;
      }
        
      if (firstchar == EXCLAMATION) {  /* treat line as a host command */
        nialptr     x = apop();
        mkstring(pfirstchar(x) + (i + 1));  /* creates a Nial string from the
                                command with the exclamation symbol removed */
        ihost();
        freeup(apop());
        freeup(x);
        goto retry;
      }
   


     /* done handling the special cases */
        
     {
        /* analyze and execute the input line */


        iscan();       /* scan the input line */

        parse(true);   /* parse the tokens */

#ifdef DEBUG
        if (debug) {
          nprintf(OF_DEBUG, "memcheck in mainloop before ieval\n");
          memchk();
          nprintf(OF_DEBUG, "done\n");
        }
#endif
         
        if (lastval != -1) {
             decrrefcnt(lastval);
             freeup(lastval);
             lastval = -1;
        }

        ieval();       /* evaluate the parse tree */

          /* need following test to exit latent called from a startup load */
         if (nomainloop)
             goto cleanup;
         
        if (latentfound) {
            latentfound = false;
            doinglatent = false;
        }
            

        init_debug_flags(); /* reset up flags for Nial level debugging  */
        
        /* remember last toplevel value for history */
     
        if (lastval != -1) {
          decrrefcnt(lastval);
          freeup(lastval);
        }
        lastval = top;
        incrrefcnt(lastval); /* protect lastval so it is available for ]<var> request */
        
#ifdef DEBUG
        if (debug) {
          nprintf(OF_DEBUG, "memcheck in mainloop after ieval\n");
          memchk();
          nprintf(OF_DEBUG, "done\n");
        }
#endif

        if (top != Nullexpr) {
          ipicture();  /* prepare the output picture */

#ifdef DEBUG
          if (debug) {
            nprintf(OF_DEBUG, "memcheck in mainloop after ipicture\n");
            memchk();
            nprintf(OF_DEBUG, "done\n");
          }
#endif
          
          show(apop());  /* call routine that displays the picture,
                        wrapping it if necessary */
          
#ifdef DEBUG
          if (debug) {
            nprintf(OF_DEBUG, "memcheck in mainloop after display\n");
            memchk();
            nprintf(OF_DEBUG, "done\n");
          }
#endif
 
        }
        else
          apop(); /* remove the Nullexpr. No need to test it as free. */

#ifdef DEBUG
    
         /* done evaluation steps. check stack is empty */
       
        if (topstack != -1) {
          /* stack not empty here indicates an internal error in the interpreter */
          while (topstack >= 0)
            freeup(apop());
          exit_cover1("evaluation stack not empty",NC_FATAL);
        }
#endif
         
      }
    
      /* check if there was a Control C press during CI execution */
       
      if (userbreakrequested) {
        
#ifdef DEBUG
        nprintf(OF_DEBUG, "userbreakrequested set during input at top level loop\n");
#endif
        
        userbreakrequested = false;
        goto retry;            /* to go around to the prompt again */
      }
    }
  }
    
  /* last input entry has been processed. Go back to the beginning 
     of the top level loop */
  
  goto retry;

  /* end of top level loop code */

cleanup:

  /* stop the interpreter and clean up its resource usage */

    /* If the interpreter is running set the flag to false,
     close the user files and clear the abstract machine.
     In either case, destroy the call stack.
     */
 

    closeuserfiles();        /* close all user files */
    
    /* not sure we need the next two lines. */
    clear_abstract_machine();
    destroy_call_stack();

  /* return from the console version of Q'Nial */

  exit(0);
  return 1;                  /* to satisfy syntax error checking */
}


/* routine to display the command line syntax when -h is used */


static void
print_syntax()
{
  fprintf(stderr, "\n"
  "SYNTAX: nial  [(+|-)size Wssize] [-defs Filename] [-i] [-lws WSName] [-h]\n"
  "\n"
  "-size Wssize\n"
  "      Begin with a workspace size of Wssize words. A suffix of G, M or K\n"
  "      can be used to indicate Giga words, Mega words or Kilo words respectively.\n"
  "      The workspace expands if space is available.\n"
  "+size Wssize\n"
  "      Fix the workspace size at Wssize words with no expansion.\n"
  "-defs Filename\n"
  "      After loading the initial workspace the file Filename.ndf\n"
  "      is loaded and executed without displaying input lines.\n"
  "-lws  Wsname\n"
  "      A previously saved workspace file is loaded on startup.\n"
  "-i\n"
  "      Execute in interactive mode with a top level loop.\n"
  "-h\n"
  "      Display command line syntax (this text).\n"
  "\n"
  "Examples:\n"
  "   nial -i\n"
  "   nial -defs app.ndf\n"
  "   nial +size 50M -defs newfns\n"
  );
}

/* get_memsize computes the size of the heap memory from the input string provided
 * with the -size or +size options.
 * The size string can include an M for megabytes, G for gigabytes or K for kilobytes
 */

#define string_to_nialint(s) atol(s)

static nialint get_memsize(char *mstr) {
    char msz[128];
    int mpl = strlen(mstr);
    nialint mult = 1;      /* work with default of words */
    char term = mstr[mpl-1]; /* get Last char to see if it is G or M or K */
    
    strcpy(msz, mstr);
    if (term == 'K' || term == 'k') { /* kilowords */
      msz[mpl-1] = '\000';
      mult = 1024;
    } else if (term == 'M' || term == 'm') { /* megawords */
      msz[mpl-1] = '\000';
      mult = 1024*1024;
    } else if (term == 'G' || term == 'g') { /* Gigawords */
      mult = 1024*1024*1024;
      msz[mpl-1] = '\000';
    }
    
    /* compute size and return it */
    return mult*string_to_nialint(msz);
}

extern void
cleanup_ws()
{
    /* cleanup common to all other longjmp calls */
    
    cleardeffiles();           /* clean up open ndf files */
    clearstack();              /* clear the stack and list of temp arrays */
    clearheap();               /* remove all arrays with refcnt 0 */
    closeuserfiles();          /* close files to avoid interference */
    clear_call_stack();        /* clears names of called routines */

#ifdef PROFILE
    clear_profiler();          /* remove profiling data structures if in use */
#endif
    
    /* reset all debugging flags */
    init_debug_flags();
    trace = false;             /* to avoid it being left on accidentally */
    startfilesystem();

 /*   lastval = -1; */
    fflush(stdin);
}

/* routines to support control C catch in console version */



/* routine to invert the collating sequence for rapid comparison
 of character ordering.  */


static void
set_cseq(char *cseq)
{
    int         i;
    int         len = strlen(cseq);
    
    for (i = 0; i < HIGHCHAR - LOWCHAR + 1; i++)
        invseq[i] = i;
    for (i = 0; i < len; i++)
        invseq[cseq[i] - LOWCHAR] = i + 32;
}



/* routine to test if a Latent expression exists in the workspace */

static int
latenttest(void)
{
    nialptr     entr,
    sym,
    x;
    
    /* looks up the term Latent in the global environment */
    current_env = Null;
    x = makephrase("LATENT");
    entr = lookup(x, &sym, passive);
    freeup(x);
    return (entr != notfound) && sym == global_symtab && (sym_role(entr) == Rexpr);
}




