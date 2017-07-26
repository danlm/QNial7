/*==============================================================

  MODULE   EVAL.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This contains the evaluation mechanisms.
  The eval() routine is in eval_fun.c so that it can be
  included twice to make a debug and non-debug version.
  This allows the same executable to be run with debugging_on
  or debugging_off with only a minor speed penalty in the latter
  case.

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

/* STLIB */
#include <string.h>

/* STDLIB */
#include <stdlib.h>

/* CLIB */
#include <ctype.h>

/* MALLOCLIB */
#ifdef OSX
#include <stdlib.h>
#elif LINUX
#include <malloc.h>
#endif

/* SJLIB */
#include <setjmp.h>

/*  TIMELIB */
#include <sys/types.h>
#ifdef UNIXSYS
#include <sys/times.h>
#endif
#ifdef WINNIAL
#include <sys/time.h>
#endif
#include <sys/param.h>


/* Q'Nial header files */

#include "eval.h"
#include "absmach.h"
#include "basics.h"
#include "qniallim.h"
#include "lib_main.h"
#include "if.h"

#include "utils.h"           /* for getuppername */
#include "blders.h"          /* for get routines */
#include "getters.h"         /* for get macros */
#include "parse.h"           /* for parse tree node tags */
#include "profile.h"         /* for profile hnadling */
#include "symtab.h"          /* symbol table macros */
#include "roles.h"           /* for role codes */
#include "fileio.h"          /* for OC_DEBUG code etc. */
#include "scan.h"            /* for Lower */
#include "ops.h"             /* for splitfb and append */
#include "compare.h"         /* for equal */
#include "faults.h"          /* for fault macros */
#include "insel.h"           /* for select, insert */



/* prototypes for local routines */

static void apply_transform(nialptr tr);
static void prologue(nialptr env, nialint nvars, nialptr * senv);
static void epilogue(nialptr senv, nialint nvars);
static void setup_env(nialptr syms, nialptr sps, nialptr * svenv);
static void restore_env(nialptr syms, nialptr svenv);
static void nct_throw_result(nialptr nfault, int rc_code);


/* global variables */
static int  d_inloop = false;/* indicates currently in a loop statement */

static int  call_stack_pointer = 0; /* points at current call stack entry */


static int  d_infaultbreakloop, /* signals a resume after break */
            d_onestep,       /* signals a single step during a break */
            d_stepin,        /* traces into expression evaluation */
            d_toend,         /* controls jumping to end of loop or defn */
            d_next,          /* controls stepping to next expr */
            d_forceresume,   /* forces resumption of computation */
            d_lastcommand,   /* keeps track of last command */
            d_lastquiet,     /* indicates last command was quiet */
            d_quiet,         /* indicates current command is quiet */
            d_repeatcount;   /* number of times current command is repeated */

static void dump_call_stack(void);
static void enter_op(nialptr entr);
static void exit_op(void);
static void break_loop(int flag);
static void do_watch_action(nialptr, nialptr);
static void fault_loop(nialptr);
static int  showexpr(nialptr tree, int prompttype);

static int nct_in_catch_throw();

/*  The evaluation mechanism:

    every Nial expression denotes either
    - an array value,
    - an operation, or
    - a transformer.

    The array expressions include:
    constants
    variables
    basic named-expressions
    user-defined named-expressions
    list constructs
    control constructs
    blocks
    assignments
    operation applications
    definitions

    The operation expressions include:
    basic named operations
    user-defined named operations
    operation compositions
    atlases
    opforms e.g. (op a (a+1))
    curried operations e.g. 1+
    transforms e.g. EACH rest

    The transformer expressions include:
    basic named transformers
    user renamings of transformers
   user-defined named trforms e.g. TWICE is tr f(f f)

    For each class of semantic object there is one routine that
    handles it:
    array expressions - eval
    operation expressions - apply
    transformer expressions - apply_transform

    The evaluator is stack-based.

    eval evaluates an array expression and places the result on the stack.

    apply applies the operation expression to the value on the top of the
    stack replacing it with the result value.

    apply_transform uses an array value and an operation expression on the
    stack to implement the semantics of applying the transformer expression
    to the operation argument and then applying the resulting function
    to the array argument.

    The stack is used to hold other information as well. Blocks, opforms
    and trforms all define local scopes. When one of these is entered an
    activation record is placed on the stack which holds the backpointer to
    the previous one for this scope (-1 if there isn't one) and the value
    cells for the local names. The routine prologue sets up an activation
    record and epilogue discards it.

    In order to get the proper semantics for trforms, their operation argument
    must be "closed with its environment". This means that the pointers to
    all local scopes must be saved. When a closure is applied the stack is used
    to hold the current stack pointers while the ones bundled with the
   operation are installed. The routines setup_env and restore_env are used
    to set up for a closure application and to clean up after it.

    The stack is also used to "protect" temporary values since an array
    has its reference count increased when it is pushed and decreased
    when it is popped. This avoids accidental cleanup of the temporary
    that is placed on the stack.

    The routine "swap" interchanges the top two stack elements. It is
    often used to get a protected value on top after the main computation
    has been done so it can be unprotected.

    The three major routines of the evaluator are eval, apply, and
    apply_transform. They take as their argument a parameter corresponding
    to the code they are to evaluate. They assume that a code argument
    is "permanent" ,i.e. it is owned in some component of a program text.
    Hence these routines do not need to free up such an argument.
    Accordingly, routines that create temporary code objects (some cases 
    in apply_transform and some primitive transformers) are responsible for cleaning up such objects.
                                   
    The stack handling is as follows:
        eval(exp)    leaves its result on the stack.
        apply(op)    takes the array arg on the stack and leaves
                     the result on the stack. It frees up the argument
                     if it is temporary.
        applytr(tr)  takes the array arg on the stack (topm1) and the operation
                     arg on the stack (top).  It leaves the result on the stack.
                     It frees up the array argument if it is temporary. It does
                     not free up the operation arg since this is assumed to be
                     permanent code; if it is temporary, then the routine that
                      built it must clean it up.
*/


/* global variable */

static nialptr saveexp;      /* used to save current expression for debug
                              * loop */

static int  userroutines = 0;/* flag to display names of user routines on
                              * entry and exit */
static int  usercallchanged = 0;
static int  usercallvalue;
static int  primroutines = 0;/* flag to display names of primitive routines
                              * on entry and exit */
static int  primcallchanged = 0;
static int  primcallvalue;


static int  changetrigger,
            triggervalue;    /* used in this module to control triggered
                              * setting */



#define is_systemop(op)	(tag(op)==t_variable && sym_flag(get_entry(op)))


 /* eval routine that can be called from Nial level */

void
ieval()
{
  nialptr     nm,
              sym,
              entr;
  nialptr     tree = apop();

  /* tree is the high level expression to be evaluated. Named objects are
   protected directly in eval apply and apply_transform to permit the given
   examples in each routine to work. Thus there is no need to protect tree
   here. There is a need to protect the current environment since it gets stored
   in closure records that are freed after use. 
  */
  incrrefcnt(current_env);
  switch (kind(tree)) {
    case faulttype:
        apush(tree);         /* pass faults through as the result */
        break;
    case phrasetype:
        nm = tree;           /* lookup expression name */
        if (varname(nm, &sym, &entr, false, false)) {
          if (sym_role(entr) != Rexpr) {
            apush(makefault("?not an expression"));
          }
          else
            eval(fetch_var(sym, entr));
        }
        else
          apush(makefault("?not a name"));
        break;
    case atype:              /* tree should be a parse tree */
        if (tally(tree) != 0 && ptag(tree) == t_parsetree) {
          nialptr     body;

          body = pbody(tree);
          eval(body);
          break;
        }
        /* else fall through to default */
    default:
        apush(makefault("?eval needs parse tree as argument"));
  }
  freeup(tree);              /* frees the parse trees generated in the top
                               level loop and in execute. */
  decrrefcnt(current_env);
}

/* compile the two version of eval, with and without Nial level debugging 
   enabled. This approach reduces the overhead when Nial debugging is
   turned off.
*/

#define EVAL_DEBUG
#include "eval_fun.c"
#undef EVAL_DEBUG
#include "eval_fun.c"

/* we choose between them with a macro (defined in eval.h)
    unless in  C level debug version  */



#ifdef DEBUG
void
eval(nialptr exp)
{
  if (debugging_on)
    d_eval(exp);
  else
    n_eval(exp);
}

#endif

/* apply applies an operation to an array */

void
apply(nialptr fn)
{
#ifdef DEBUG
  if (top <= 0 || top >= memsize) {
    nprintf(OF_DEBUG, "invalid arg of apply %d tag %d\n", top, tag(top));
    nabort(NC_ABORT);
  }
#endif

  /* if a signal has been indicated go to top level */
#ifdef USER_BREAK_FLAG
  checksignal(NC_CS_NORMAL);
#endif

  /* Check if the stack at this point (MACRO) */
  if (CSTACKFULL)
    longjmp(error_env, NC_WARNING);

recur:
  switch (tag(fn)) {
    case t_variable:         /* code tree denotes an operation name */
        {
          nialptr     op,
                      entr,
                      sym;
          int         localtrace = trace;
          int         namestacked = false;
          int         local_onestep = d_onestep;
          int         local_stepin = d_stepin;
          int         stepskipped = false;

#ifdef PROFILE
          int         pnamestacked = false;
#endif
          /* get the operation code */
          entr = get_entry(fn);
          sym = get_sym(fn);
          op = fetch_var(sym, entr);

          if (d_next || is_systemop(fn)) {
            d_onestep = false;
            d_stepin = false;
            stepskipped = true;
          }

#ifdef PROFILE
          if (profile && !sym_flag(entr) && sym == global_symtab) {
            profile_ops_start(entr);
            pnamestacked = true;
          }
#endif
          if ((triggered || debugging_on) && !sym_flag(entr)) {
            /* add operation to call stack */
            enter_op(entr);
            namestacked = true;
          }
          if (sym_trflg(entr)) {
            nprintf(OF_NORMAL_LOG, "...trace call to operation\n");
            showexpr(fn, TRACE);
            trace = true;
          }
          else               /* suspend trace if it is on */
            trace = false;
          if (debugging_on)
            if (sym_brflg(entr)) {  /* flag is on for breaking in this
                                       operation */
              ibreak();
              apop();
            }

          /* to protect op in case it is redefined , e.g. foo is op a {
           * execute 'foo is op b{b + 1}'; } */
          apush(op);
          swap();            /* put arg back on top */
          /* do the application of the operation */
          apply(op);
          swap();            /* unprotect and cleanup op */
          freeup(apop());
          trace = localtrace;
#ifdef PROFILE
          if (pnamestacked)
            profile_ops_stop(entr);
#endif
          if (namestacked)
            /* remove the operation from the call stack */
            exit_op();

          if (stepskipped) {
            d_onestep = local_onestep;
            d_stepin = local_stepin;
          }

          break;
        }

    case t_basic:            /* apply unary basic operation */
        {
          APPLYPRIMITIVE(fn);
#ifdef FP_EXCEPTION_FLAG
          fp_checksignal();
#endif
        }
        break;

    case t_opform:           /* apply operation form */
        {
          nialptr     save_env,
                      args = get_arglist(fn),
                      body = get_body(fn);
          nialptr     body_expr,
                      val;
          nialint     nvars = get_cnt(fn);
          int         bsw = tag(body) == t_blockbody;

          val = apop();      /* argument of the operation call */
          prologue(get_env(fn), nvars, &save_env);  /* set up the local env. */
          if (bsw) {         /* the body is a block */
            nialptr     defs = get_defs(body);

            body_expr = get_seq(body);
            nonlocs = get_nonlocallist(body);
            if (defs != grounded) {
              eval(defs);
              apop();        /* result is Nullexpr and is ignored */
            }
          }
          else {             /* body is just an expression */
            body_expr = body;
            nonlocs = Null;  /* needed by lookup */
          }
          /* assign the arg vals to the parameter names */
          if (!assign(args, val, false, false)) { /* assign fails if number
                                                     of parameters differs
                                                     from the length of the arg */
            apush(makefault("?op_parameter"));
          }
          else {
            if (trace) {
              nprintf(OF_NORMAL_LOG, "...the arguments for the opform are\n");
              showexpr(args, TRACE);
            }
            /* evaluate the body expression */
            eval(body_expr);
          }
          if (trace)
            nprintf(OF_NORMAL_LOG, "...end of operation call \n");
          epilogue(save_env, nvars);  /* restore the environment */
        }
        break;

    case t_composition:      /* a composition is a sequence of operations to
                                be applied right to left */
        {
          nialint     noptns,
                      i;

          noptns = tally(fn) - 1;
          for (i = noptns - 1; i >= 0; i--) /* apply operations right to left */
            apply(fetch_array(fn, i + 1));
        }
        break;

    case t_curried:          /* unevaluated left argument used to form a
                                pair. This case is still needed when a
                                curried operation is called from a
                                transformer */
        {
          nialptr     arg1;
          int         flag = kind(top) >= phrasetype;

          if (flag)
            apush(top);      /* to protect the argument in weird cases. This
                                is necessary because a phrase or fault may be
                                shared without its ref count being set. This
                                was the source of a very subtle error. */
          eval(get_argexpr(fn));  /* evaluate the left argument */
          arg1 = apop();
          pair(arg1, apop());/* place the pair on the stack */
          apply(get_op(fn));
          if (flag) {
            swap();
            freeup(apop());  /* free arg if it is temporary */
          }
        }
        break;

    case t_vcurried:         /* curried on evaluated left arg */
        /* form the pair from the curried value and the argument on the stack
           leaving the pair on the stack */
        pair(get_argval(fn), apop());
        fn = get_op(fn);
        goto recur;

    case t_atlas:            /* atlas of operations 
                                [f, g, ...] A = [f A, g A, ...] */
        {
          nialint     i,
                      cnt;
          nialptr     arg = top;  /* to protect arg for several calls */

          cnt = tally(fn);
          for (i = 1; i < cnt; i++) { /* apply the operations in the atlas in
                                       * left to right order */
            apush(arg);
            apply(fetch_array(fn, i));
          }
          /* results are on the stack */
          mklist(cnt - 1);
          swap();
          freeup(apop());    /* to free arg */
        }
        break;

    case t_transform:        /* apply a transform */
        {
          nialptr     op = get_argop(fn),
                      tr,
                      closure = Null; /* initialized to avoid warning */
          int         done;
          int         local_inloop = d_inloop;

          d_inloop = false;

          tr = get_tr(fn);
          apush(op);         /* push the operation on the stack */
#ifdef PROFILE
          if (!(triggered || debugging_on || profile))
#else
          if (!triggered || debugging_on)
#endif
            coerceop();      /* dereferences op to make atlases visible. This
                                is necessary to get expected semantics for
                                for transformers that expect an atlas. We do
                                it here for efficency. However if we are
                                triggering on faults, the coercion will strip
                                off defined names and the operation will not
                                show up in the call stack. If triggering is
                                on, then the routines that expect atlases do
                                the coercion directly. */
          done = b_closure();/* close the operation on the stack */
          if (done) {
            incrrefcnt(top);
            closure = top;   /* to preserve the closure */
          }
          apply_transform(tr);
          if (done) {
            decrrefcnt(closure);
            freeup(closure); /* clean up the closure */
          }
          d_inloop = local_inloop;
        }
        break;

    case t_closure:          /* apply an operation in a closure */
        {
          nialptr     syms,
                      sps,
                      svenv = -1; /* assigned to avoid compiler warning */

          syms = get_env(fn);/* list of environments */
          sps = get_sps(fn); /* list of stack pointers */
          setup_env(syms, sps, &svenv); /* set up the closed environment */
          apply(get_op(fn));
          restore_env(syms, svenv); /* restore environment after apply */
        }
        break;

    case t_parendobj:
    case t_dottedobj:
        /* get the operation and apply it avoiding the recursive call */
        fn = get_obj(fn);
        goto recur;
    default:
#ifdef DEBUG
        nprintf(OF_DEBUG, "invalid case in apply\n");
        nprintf(OF_DEBUG, "tag is %d\n", tag(fn));
        nabort(NC_ABORT);
#else
        freeup(apop());      /* to remove argument */
        apush(makefault("?invalid case in apply"));
#endif
  }
#ifdef DEBUG
  if (top <= 0 || top >= memsize) {
    nprintf(OF_DEBUG, "invalid result of apply %d tag %d\n", top, tag(fn));
    nabort(NC_ABORT);
  }
#endif
}

/* routine to apply a transform */

static void
apply_transform(nialptr tr)
{
#ifdef DEBUG
  if (top <= 0 || top >= memsize) {
    nprintf(OF_DEBUG, "invalid arg of apply_transform %d tag %d\n", top, tag(top));
    nabort(NC_ABORT);
  }
#endif
  switch (tag(tr)) {
    case t_variable:         /* a named transformer */
        {
          nialptr     val,
                      trf;
          int         localtrace = trace;
          int         namestacked = false;

#ifdef PROFILE
          int         pnamestacked = false;

#endif
          nialptr     entr,
                      sym;

          trf = fetch_var(get_sym(tr), get_entry(tr));
          val = apop(); /* pick up argument */
          /* protect tr in case it gets redefined, e.g. FOOL is tr f op a {
           * execute 'FOOL is tr f op b{f (b + 1)}'; } */
          apush(trf);
          swap();            /* to put it below the function value */
          apush(val);  /* replace argument */
          entr = get_entry(tr);
          sym = get_sym(tr);
          if (sym_trflg(entr)) {
            nprintf(OF_NORMAL_LOG, "...trace call to transform ");
            showexpr(tr, TRACE);
            trace = true;
          }
          else               /* suspend trace if it is on */
            trace = false;
          if (debugging_on)
            if (sym_brflg(entr)) {  /* flag is on for breaking in this transform */
              ibreak();
              apop();
            }
#ifdef PROFILE
          if (profile && !sym_flag(entr) && sym == global_symtab) {
            profile_ops_start(entr);
            pnamestacked = true;
          }
#endif
          if ((triggered || debugging_on) && !sym_flag(entr)) {
            enter_op(entr);
            namestacked = true;
          }
          /* do the work of the application */
          apply_transform(trf);
          /* now cleanup tr */
          swap();
          freeup(apop());
          trace = localtrace;
#ifdef PROFILE
          if (pnamestacked)
            profile_ops_stop(entr);
#endif
          if (namestacked)
            exit_op();
          break;
        }

    case t_basic:            /* a primitive transformer */
        {
          APPLYPRIMITIVE(tr);
#ifdef FP_EXCEPTION_FLAG
          fp_checksignal();
#endif
        }
        break;


    case t_trform:           /* apply transformer form */
        {
          nialptr     fval,
                      args,
                      trbody,
                      save_env,
                      val;
          nialint     nargs;
          int         closureflag = false;

          fval = apop();
          val = apop();
          args = get_opargs(tr);
          nargs = tally(args) - 1;
          if (nargs > 0)
            prologue(get_env(tr), nargs, &save_env);  /* set up the local env */
#ifdef PROFILE
          if (nargs > 1 && (triggered || debugging_on || profile))
#else
          if (nargs > 1 && (triggered || debugging_on))
#endif
          {   /* We must coerce the oparg here to get the right semantics. 
                 This will still hide the use of functions whose names 
                 are coerced out. */
            if (tag(fval) == t_closure) { 
              /* if a closure was done then it must be redone after the
                 coercion so that an atlas won't be hidden. */
              nialptr     op = get_op(fval);

              apush(op);
              coerceop();
              closureflag = b_closure();
              freeup(fval);
              if (closureflag) {
                incrrefcnt(top);  /* to protect closure */
              }
              fval = apop();
            }
            else {
              apush(fval);
              coerceop();
              fval = apop();
            }
          }
          /* assign the operation(s) to the transformer parameter(s) */
          if (!assign(args, fval, true, false)) {
            apush(makefault("?tr_parameter"));
          }
          else { /* push the argument and apply to transformer body */
            trbody = get_trbody(tr);
            apush(val);
            apply(trbody);
          }
          if (nargs > 0)
            epilogue(save_env, nargs);  /* restore argument */
          if (closureflag) {
            decrrefcnt(fval);
            freeup(fval);
          }
        }
        break;


    default:
#ifdef DEBUG
        nprintf(OF_DEBUG_LOG, "invalid case in transform apply\n");
        nprintf(OF_DEBUG, "tag is %d\n", tag(tr));
        nabort(NC_ABORT);
#else
        freeup(apop());
        freeup(apop());      /* to get rid of fn and arg on stack */
        apush(makefault("?invalid case in transform apply"));
#endif
  }
#ifdef DEBUG
  if (top <= 0 || top >= memsize) {
    nprintf(OF_DEBUG, "invalid result of apply_transform %d tag %d\n", top, tag(tr));
    nabort(NC_ABORT);
  }
#endif
}


/* eval.c auxiliary routines */

/* these routines are used to call primitives in debugging mode so
that we can optionally print the names. Controlled by a macro.
*/

void
applyprimitive(nialptr p)
{
  if (primroutines) {
    int         ind = get_index(p);
    char       *tmp_str = slower(pfirstchar(bnames[ind]));

    ((*applytab[ind]) ());
    nprintf(OF_NORMAL, "executed %s\n", tmp_str);
    free(tmp_str);
  }
  else
    ((*applytab[get_index(p)]) ());
}

void
applybinaryprim(nialptr p)
{
  if (primroutines) {
    int         ind = get_binindex(p);
    char       *tmp_str = slower(pfirstchar(bnames[ind]));

    /* why are we doing this redundant call?... See comment in applyprimitive */

    ((*binapplytab[ind]) ());
    nprintf(OF_NORMAL, "executed %s\n", tmp_str);
    free(tmp_str);
  }
  else
    ((*binapplytab[get_binindex(p)]) ());
}

/* routine to fetch a variable or named code object from its value
   cell in the dynamic environment. For a global name the value is in
   the symbol table. For a local name the value is in the activation
   record pointed at by the stack pointer for the local symbol table
   at the offset given in the value position in the local table.
*/

nialptr
fetch_var(nialptr sym, nialptr entr)
{
  nialint     cursp,
              offset;

  if (sym == global_symtab)
    return (sym_valu(entr));
  else {
    cursp = get_spval(sym);
    offset = intval(sym_valu(entr));
    if (cursp == -1)
      return (makefault("?variable out of context"));
    else
      return (stkarea[cursp + offset]);
  }
}

/* routine to store a value in a name. used by assign. */

int
store_var(nialptr sym, nialptr entr, nialptr v)
{
  nialptr     oldv;
  nialint     cursp,
              offset;

  if (sym == global_symtab) {
    st_s_valu(entr, v);
    if (debugging_on && watchlist != Null) {
      do_watch_action(global_symtab, entr);
    }

    return (true);
  }
  cursp = get_spval(sym);
  offset = intval(sym_valu(entr));
  if (cursp != -1) {         /* there is a current activation */
    oldv = stkarea[cursp + offset];
    stkarea[cursp + offset] = v;
    incrrefcnt(v);
    if (debugging_on && watchlist != Null) {
      do_watch_action(sym, entr);
    }
    decrrefcnt(oldv);
    freeup(oldv);            /* do this after store to avoid interference */
    return (true);
  }
  return (false);
}


/* routine to implement the binding of the vars to the values. If the varlist
   is of length one then the var is bound to val; otherwise the
   number of formal arguments must match the number of items in the
   value.
   If trsw is set the assignment is in a trform argument binding.
   (and we must expect the tag field t_atlas if nvars > 1).
   It must allow for a atlas in doing the splitting.
   Since it is possible for a name in a list being updated to appear on the
   right, we must protect the value being updated to avoid it being
   freed up.
*/

int
assign(nialptr varlist, nialptr val, int trsw, int valneeded)
{
  nialptr     var,
              x;
  int         i,
              nvars,
              res;

  apush(val);                /* to protect val */
  nvars = tally(varlist) - 1;
  if (nvars == 1) {
    var = fetch_array(varlist, 1);
    res = store_var(get_sym(var), get_entry(var), val);
  }
  else {
    if (trsw) {
      nialptr     oparg;

      oparg = val;
      if (tag(oparg) != t_atlas || nvars != tally(oparg) - 1) {
        valneeded = false;
        res = false;
        goto cleanup;
      }
      for (i = 0; i < nvars; i++) {
        x = fetchasarray(oparg, i + 1); /* + 1 allows for the tag */
        var = fetch_array(varlist, i + 1);  /* + 1 allows for the tag */
        store_var(get_sym(var), get_entry(var), x);
      }
      res = true;
    }
    else {
      if (nvars != tally(val)) {
        valneeded = false;
        res = false;
        goto cleanup;
      }
      for (i = 0; i < nvars; i++) {
        x = fetchasarray(val, i);
        var = fetch_array(varlist, i + 1);  /* + 1 allows for the tag */
        store_var(get_sym(var), get_entry(var), x);
      }
      res = true;
    }
  }
cleanup:
  apop();                    /* protected value */
  if (!valneeded)
    freeup(val); /* removes temporaries used in assign in for loop */
  return (res);
}


/* routine to enter a new environment.
  sets up the static environment appropriate to the body to be evaluated and
   extends the dynamic environment, adding a new local environment
   corresponding to the first sym in the given environment list. The current
   stack pointer for that sym table is saved on the stack and reset to the
   top of stack.
   On completion of prologue the stack will contain:
                   . . .
                 space for the result
                 OLDSP for symbol table sym
   cursp(sym)--> value cell for var 0
                . . .
   topstack----> value cell for var nvars-1
*/

static void
prologue(nialptr env, nialint nvars, nialptr * senv)
{
  int         i;
  nialptr     newsym;

  *senv = current_env;       /* save the environment in the calling C
                              * environment */
  current_env = env;
  apush(no_value);           /* reserve space for the result */
  newsym = fetch_int(env, 0);/* get the new symbol table number */
  apush(get_sp(newsym));     /* push the old stack pointer value */
  store_sp(newsym, createint(topstack + 1));  /* store new stack pointer */
  for (i = 0; i < nvars; i++)
    apush(no_value);         /* initialize the local names */
}

/* routine to undo the work of prologue */

static void
epilogue(nialptr senv, nialint nvars)
{
  int         i;
  nialptr     newsym = fetch_array(current_env, 0); /* get the symbol table no */

  current_env = senv;        /* restore current environment */
  decrrefcnt(no_value);      /* remove refcnt on fault in result posn */
  movetop(nvars + 2);        /* move the result to its reserved space */
  for (i = 0; i < nvars; i++)
    freeup(apop());          /* free and deallocate local variable */
  store_sp(newsym, apop());  /* store the old stack pointer */
}


/*  routine to implement the setrigger operation */

void
isettrigger(void)
{
  nialptr     x = apop();
  int         oldstatus = triggered;

  if (atomic(x) && kind(x) == booltype)
    triggervalue = boolval(x);
  else if (atomic(x) && kind(x) == inttype)
    triggervalue = intval(x);
  else {
    apush(makefault("?settrigger expects a truth-value"));
    freeup(x);
    return;
  }
  changetrigger = true;
  apush(createbool(oldstatus));
}


/* routine that displays an expression as text for trace
   and in support of debugging */

static int
showexpr(nialptr tree, int prompttype)
{
  nialptr     text;
  nialptr     firstline;
  nialint     i;
  char        prmt[20];

  prmt[0] = (char) 0;
  if (tree == Nulltree)
    return (false);

  if (prompttype == QUESTION)
    strcat(prmt, "?.. ");
  else
    strcat(prmt, "... ");

  apush(b_parsetree(tree));  /* ideparse requires parse tree. It will free
                              * the temporary tree */
  ideparse();
  idescan();
  text = apop();
  if (tally(text) != 0) {
    mkstring(prmt);
    pair(apop(), fetch_array(text, 0));
    ilink();
    firstline = apop();
    show(firstline);
    for (i = 1; i < tally(text); i++)
#ifdef DEBUGLINES
    {
      if (i >= DEBUGLINES)
        continue;
      mkstring("    ");
      pair(apop(), fetch_array(text, i));
      ilink();
      show(apop());
    }
    if (tally(text) > DEBUGLINES)
      nprintf(OF_NORMAL_LOG, "       (...)\n");
#else
      show(fetch_array(text, i));
#endif
    freeup(text);
    return (false);
  }
  else
    return (true);
}


/* routine that implements the setdeftrace operation that allows
  tracing of individual named expressions and operations.
*/

void
isetdeftrace(void)
{
  nialptr     x,
              sym,
              entr,
              nm,
              newnm,
              flg;
  nialint     res;
  int         test;

  x = apop();
  if (tally(x) == 2) {
    splitfb(x, &nm, &flg);
    if (kind(nm) != chartype && kind(nm) != phrasetype) {
      buildfault("invalid first argument in setdeftrace");
      goto cleanup;
    }
    if (kind(flg) != booltype && kind(flg) != inttype) {
      buildfault("invalid second argument in setdeftrace");
      goto cleanup;
    }
    test = ((kind(flg) == booltype && boolval(flg)) ||
            (kind(flg) == inttype && intval(flg) == 1));
    apush(nm);
    freeup(flg);
    freeup(x);
  }
  else {                     /* assume x is the name and hence toggle the
                              * setting */
    if (kind(x) != chartype && kind(x) != phrasetype) {
      buildfault("invalid argument in setdeftrace");
      freeup(x);
      return;
    }
    apush(x);
    test = -1;
  }
  newnm = getuppername();
  entr = lookup(newnm, &sym, passive);
  if (entr == notfound)
    buildfault("undefined name");
  else {
    nialptr     op = sym_valu(entr);  /* check if the argument is an atlas */

    while (tag(op) == t_variable)
      op = fetch_var(get_sym(op), get_entry(op));
    if (tag(op) == t_atlas) {
      buildfault("cannot trace a named atlas");
      /* we cannot trace a renamed atlas because op coercion does not go
       * through a traced name, but must find the atlas referred to by a
       * name. */
      freeup(newnm);
    }
    if (sym_flag(entr)) {
      buildfault("cannot trace a predefined name");
      freeup(newnm);
    }
    res = sym_trflg(entr);   /* get the trace flag */
    if (test == -1) {        /* toggle it */
      st_s_trflg(entr, !res);
    }
    /* block required due to macro handling */
    else {                   /* set it to the given value */
      st_s_trflg(entr, test);
    }                        /* block required due to macro handling */
    apush(createint(res));
  }

  freeup(newnm);
  return;
cleanup:
  freeup(nm);
  freeup(flg);               /* in case they are temporaries */
  freeup(x);
}


/* b_closure is the routine that records the environment at the point of
  call of a transformer applied to an operation. The operation
  must be evaluated in the environment at the point of
  the call when applied in the body of the transformer
  definition.

  If there is no current local environment or if the operation is
  basic or globally defined, there is no need to build a closure
  record.

  Since we must not hide an atlas, (otherwise the trform parameter
  mechanism fails), we build a new atlas from the closure of its
  components.

  We avoid building a closure if it would have no effect in the
  case of compositions, atlases and vcurried.

  For all other operations, the correct meaning is picked up
  by saving the stack pointers for the current activations
  and the current environment (a list of symbol tables)
  so that when the operation is applied, the environment can be
  restored temporarily.

*/

int
b_closure()
{
  nialptr     farg,
              sym,
              res,
              sp,
              sps;
  nialint     atly,
              i,
              t;
  int         changed = false;


  if (current_env == Null)   /* no environments to close */
    return false;
  switch (tag(top)) {
    case t_basic:
        return false;        /* no need to close a basic */
    case t_variable:
        {
          nialptr     sym,
                      entr,
                      op;

          sym = get_sym(top);
          if (sym == global_symtab)
            return false;    /* no need to close a global named-op */
          entr = get_entry(top);
          op = fetch_var(sym, entr);
          if (tag(op) == t_basic ||
              (tag(op) == t_variable && get_sym(op) == global_symtab)) {
            freeup(apop());
            apush(op);
            return false;    /* no need to close a renaming of a basic or a
                              * global named-op. Strip off the extra layer */
          }
        }
        break;

    case t_composition:
        {
          farg = apop();
          atly = tally(farg);
          apush(createint(t_composition));
          goto join_atlas;
        }

    case t_atlas:
        {
          farg = apop();
          atly = tally(farg);
          apush(createint(t_atlas));
      join_atlas:
          /* construct composition or atlas of closures */
          for (i = 1; i < atly; i++) {
            nialptr     fit = fetch_array(farg, i);
            int         closuredone;

            apush(fit);
            closuredone = b_closure();
            changed = closuredone || changed;
          }
          mklist(atly);

          if (!changed) {    /* no closures actually done */
            freeup(apop());  /* throw away constructed version */
            apush(farg);     /* return old version */
            return false;
          }
          return true;
        }

    case t_vcurried:
        {
          nialptr     op = get_op(top);

          if (tag(op) == t_basic ||
              (tag(op) == t_variable && get_sym(op) == global_symtab))
            return false;    /* no need to close a vcurried basic or named-op */
        }
        break;
        /* other vcurried tags fall through and do the closure */
  }
  farg = apop();
  /* make a list of the stack pointers for the current environment */
  sps = new_create_array(atype, 1, 0, shpptr(current_env, 1));
  /* stack pointers */
  t = tally(sps);
  for (i = 0; i < t; i++) {
    sym = fetch_int(current_env, i);
    sp = get_sp(sym);
    store_array(sps, i, sp);
  }
  /* build the closure */
  res = mkaquad(createint(t_closure), farg, current_env, sps);
  apush(res);
  return true;
}


/* routine to use information in a closure to setup environment
   for use of the closed operation. */

static void
setup_env(nialptr syms, nialptr sps, nialptr * svenv)
{
  nialptr     sym,
              sp;
  nialint     t,
              i;

  if (syms != Null) {
    t = tally(syms);
    *svenv = current_env;
    current_env = syms;
    for (i = 0; i < t; i++) {
      sym = fetch_int(syms, i);
      apush(get_sp(sym));    /* push the saved stack pointer */
      swap();                /* to put the arg on top */
      sp = fetch_array(sps, i); /* get the sp from the closure */
      store_sp(sym, sp);     /* and install it */
    }
  }
}

/* routine to clean up after a closed operation is applied */

static void
restore_env(nialptr syms, nialptr svenv)
{
  nialptr     sym;
  nialint     t,
              i;

  if (syms != Null) {
    t = tally(syms);
    for (i = t - 1; i >= 0; i--) {
      sym = fetch_int(syms, i);
      swap();                /* to move result down 1 in the stack */
      store_sp(sym, apop());
    }
  }
  current_env = svenv;
}

/* routine that dereferences the argument of a
   transformer prior to passing it to a basic transformer
   or a defined one. This is necessary so that a transformer that
   requires an atlas, will see the atlas rather than a name
   referring to one. */

void
coerceop()
{
  nialptr     op = apop();

  /* remove any parentheses */
  while (tag(op) == t_parendobj || tag(op) == t_dottedobj)
    op = get_obj(op);
  /* dereference the argument */
  while (tag(op) == t_variable) {
    if (sym_trflg(get_entry(op))) /* do not coerce past an operation to be traced. 
                                      Not sure this changes the semantics */
      break;
    op = fetch_var(get_sym(op), get_entry(op));
  }
  apush(op);
}


/* routine that implements the primitive operation execute */

void
iexecute()
{ iscan();
  apush(top); /* to protect the tkns */
  parse(true);               /* note that iparse uses parse(false). Here we
                                want only to parse actions, whereas parse in
                                general can also parse operations or transformers 
                             */
  if (kind(top)==faulttype)  /* if it faults then return the same fault as iparse()
                                would so identities hold. */
  { nialptr res = apop();
    parse(false); /* parse the saved tokens on the stack again */
    if (kind(top)==faulttype)
    { freeup(res); /* leave fault on stack */
    }
    else
    { freeup(apop());  /* free the second parsed result and push the first one */
      apush(res);
    }
  }
  else
  { swap(); freeup(apop()); /* remove copy of tokens  leaving parsed code on stack */
    ieval();
  }
}

/* routine to implement the tonumber operation.
   This does scan and parse on it. A version that
   used atoi or atof directly would be faster. 
   If the routine is modified, it must produce the same numeric value
   generated by scan and fault in the same way. */

void
itonumber(void)
{
  if (kind(top) != chartype) 
  { if (kind(top) != phrasetype)
    { freeup(apop());
      buildfault("tonumber arg is not a string holding a number");
      return;
    }
    istring(); /* convert the argument to a string */
  }
  iscan();
  parse(true);
  {
    nialptr     tree,
                exp,
                val;

    tree = apop();
    if (ptag(tree) == t_parsetree) {
      exp = pbody(tree);
      /* This extra code is necessary to peel away the extra level of parse
       * tree generated to assist debugging design. */
      if ((tag(exp) == t_exprseq) && (tally(exp) == 2)) {
        exp = fetch_array(exp, 1);
        if (tag(exp) == t_constant) {
          val = get_c_val(exp);
          if (numeric(kind(val))) {
            apush(val);
            freeup(tree);
            return;
          }
        }
      }
    }
    apush(makefault("?not a number"));
    freeup(tree);
  }
}

/* routine to implement the primitive operation getname which converts a 
   variable reference from a parse tree to its print value in upper case.
*/

void
igetname(void)
{
    nialptr x;
    x = apop();
    apush(get_var_pv(x));
  freeup(x);
}

/* the debugging support routines follow. They are called from eval_fun.c
   if debugging_on is set.
*/


/* local global variables for call stack */
static int  call_stack_size = 100;
static nialptr *call_stack;
static int  call_stack_allocated = 0;

/* prototypes for local routines */

/* debug commands */
static nialptr dstep(int);
static nialptr dstepin(int);
static nialptr dtoend(int);
static nialptr dnext(int);
static nialptr dresume(int);

/* list maintenance routines (watch list and break list) */
static void d_add_break(nialptr);
static nialptr d_add_watch(nialptr, nialptr, nialptr);
static nialptr d_remove_watch(nialptr, nialptr);

/* routines for saving and restoring debug flags */
static debug_flags_ptr save_flags(void);
static void restore_flags(debug_flags_ptr);
static void reset_debug_flags(void);


static void executelastdebugcommand(void);
static int  executeDebugCommand(char *);


/* The structure debugcommandlist holds a linear lookup list of debug 
   commands, and their corresponding C functions (stored as function pointers).
   NOTES: 1. Each of the commands accepts an integer argument.
   It indicates how many time the command should be repeated before
   returning the user to a debug command prompt. The repeat is
   controlled by the variable d_repeatcount which is set when the
   command is first executed and decremented in subsequent calls
   to use it.
          2. The commands that have v on the end indicate that the
   value should be shown as well. They call the debug routine with the
   paramter set to true.

   All functions listed in the struct below are declared as follows:
     nialptr myfunction(int quiet)
*/

typedef  nialptr(*debug_fun_type) (int);

static struct dcl
{
  char       *name;          /* the name of the command */
  int         allowreps;     /* this command can be repeated using an numeric
                              * arg */
  int         showvalue;
  debug_fun_type fun;        /* pointer to the C function */
}           debugcommandlist[] =

{
  { "dummy", true, false, NULL },
  { "step", true, false, dstep },
  { "stepv", true, true, dstep },
  { "toend", true, false, dtoend },
  { "toendv", true, true, dtoend },
  { "resume", true, false, dresume },
  { "stepin", true, true, dstepin },
  { "next", true, false, dnext },
  { "nextv", true, true, dnext },
  { NULL, false, false, NULL }
};

/* if the above structure is adjusted, then the following defines in
   debug.h should be adjusted (if necessary) to point to the proper
   locations.
#define STEPV_LOC 2
#define TOEND_LOC 3
#define RESUME_LOC 5
 */

/* the debug command that resumes normal execution from break mode,
   unless another "break" is entered. */

static      nialptr
dresume(int dquiet)
{
  if (!debugging_on)
    return (Nullexpr);

  reset_debug_flags();
  if (d_repeatcount < 0) {
    nprintf(OF_MESSAGE_LOG, "?Count value must be a positive integer\n");
  }
  /* flag overrides */
  d_quiet = dquiet;
  d_forceresume = true;
  return (Nullexpr);
}


/* the debug command to go to end of a loop or of a definition */

static      nialptr
dtoend(int dquiet)
{
  if (!debugging_on)
    return (Nullexpr);

  reset_debug_flags();
  if (d_repeatcount < 0) {
    nprintf(OF_MESSAGE_LOG, "?Count value must be a positive integer\n");
  }
  d_quiet = dquiet;
  d_toend = true;
  return (Nullexpr);
}



/*  the debug command that sets a "one" step to be taken in break mode  */

static      nialptr
dstep(int dquiet)
{
  if (!debugging_on)
    return (Nullexpr);

  reset_debug_flags();
  if (d_repeatcount < 0) {
    nprintf(OF_MESSAGE_LOG, "?Count value must be a positive integer\n");
  }
  d_quiet = dquiet;
  d_onestep = true;
  return (Nullexpr);
}

/*  the debug command that sets a "next" step to be taken in break mode  */

static      nialptr
dnext(int dquiet)
{
  if (!debugging_on)
    return (Nullexpr);

  reset_debug_flags();
  if (d_repeatcount < 0) {
    nprintf(OF_MESSAGE_LOG, "?Count value must be a positive integer\n");
  }
  d_quiet = dquiet;
  d_onestep = true;
  d_next = true;
  return (Nullexpr);
}

/*  the debug command that sets "one inside " step to be taken in break mode  */

static      nialptr
dstepin(int dquiet)
{
  if (!debugging_on)
    return (Nullexpr);

  reset_debug_flags();
  if (d_repeatcount < 0) {
    nprintf(OF_MESSAGE_LOG, "?Count value must be a positive integer\n");
  }
  d_quiet = dquiet;
  d_onestep = true;
  d_stepin = true;
  return (Nullexpr);
}



/* resetdebugflags
  This routine is used to reset all debug flags except those that deal
  with repeated execution of a command and those that keep
   track of the last command executed.
*/

void
reset_debug_flags()
{
  d_quiet = false;
  d_forceresume = false;
  d_infaultbreakloop = false; /* breaks the fault break loop */
  d_onestep = false;
  d_toend = false;
  d_next = false;
  d_stepin = false;
}

/* routine to save the debug flags */

static      debug_flags_ptr
save_flags()
{
  debug_flags_ptr sv_flags = (debug_flags_ptr) malloc(sizeof(debug_flags_str));
  if (sv_flags == NULL)
    exit_cover1("Not enough memeory to continue", NC_FATAL);
  sv_flags->d_quiet = d_quiet;
  sv_flags->d_forceresume = d_forceresume;
  sv_flags->d_infaultbreakloop = d_infaultbreakloop;
  sv_flags->d_onestep = d_onestep;
  sv_flags->d_toend = d_toend;
  sv_flags->d_next = d_next;
  sv_flags->d_stepin = d_stepin;
  sv_flags->d_lastcommand = d_lastcommand;
  sv_flags->d_lastquiet = d_lastquiet;
  sv_flags->d_repeatcount = d_repeatcount;
  return (sv_flags);
}

/* routine to restore the debug flags */

static void
restore_flags(debug_flags_ptr oldflags)
{
  d_quiet = oldflags->d_quiet;
  d_forceresume = oldflags->d_forceresume;
  d_infaultbreakloop = oldflags->d_infaultbreakloop;
  d_onestep = oldflags->d_onestep;
  d_toend = oldflags->d_toend;
  d_next = oldflags->d_next;
  d_stepin = oldflags->d_stepin;
  d_lastcommand = oldflags->d_lastcommand;
  d_lastquiet = oldflags->d_lastquiet;
  d_repeatcount = oldflags->d_repeatcount;
  free(oldflags);
  return;
}

/* init_debug_flags 
   This routine should only be called once for every time a top level
   loop is entered. */

void
init_debug_flags()
{
  reset_debug_flags();
  d_lastcommand = STEPV_LOC;
  d_lastquiet = false;
  d_repeatcount = 0;
}

/* executeDebugCommand
   This routine examines the "line" argument, and parses out
   a command and a numeric argument.  If the commmand matches
   a command from the debugcommandlist struct, the command is
   executed, and a NON-ZERO values is returned.  The value
   indicates the index into the debugcommandlist array where
   the command was found.  If the command was not found ZERO
   is returned.
 */

int
executeDebugCommand(char *line)
{
  char       *lline;         /* local version of line arg */
  char       *command;       /* parsed out command */
  char       *argument;      /* parsed out argument value */

  int         count = 1;     /* how many times to repeat command */
  int         dquiet;        /* quiet mode "v" at end of command */
  int         cntr = 1;      /* skip the dummy in the list */

  /* duplicate the line */
  lline = strdup(line);

  /* grab the first token */
  command = strdup(strtok(lline, " \t"));

    
  /* if there is an argument, then grab it */
  argument = strtok(NULL, " \t");

  /* try to find the command in our list */
  while (debugcommandlist[cntr].name != NULL) {
    if (STRCASECMP(debugcommandlist[cntr].name, command) == 0) {
      /* first token matches a command name */
      if (argument != NULL) {
        char       *check;

        if (!(debugcommandlist[cntr].allowreps)) {
          nprintf(OF_MESSAGE_LOG, "This command does not accept an argument\n");
          free(lline);
          free(command);
          return (-1);
        }
        /* try to convert argument to a long */
        count = (int) strtol(argument, &check, 10);
        if (((check == argument) && (count == 0)) || count <= 0) {
          /* the argument was not an integer or is not negative */
          nprintf(OF_MESSAGE_LOG, "bad repeat argument to debug command\n");
          free(lline);
          free(command);
          return (-1);
        }
      }
      dquiet = !debugcommandlist[cntr].showvalue;
      d_repeatcount = count;

      /* execute the command */
      (*((debug_fun_type) debugcommandlist[cntr].fun)) (dquiet);
      d_lastcommand = cntr;
      d_lastquiet = dquiet;
      free(lline);
      free(command);
      return (cntr);
    }
    cntr++;
  }
    
  /* if execution gets here, then the line is not a debug cmd. 
     It is a request for execution */
  free(command);
  free(lline);
  return (0);
}



/* executelastdebugcommand
  Execute the last debug command.*/

static void
executelastdebugcommand()
{
  (*debugcommandlist[d_lastcommand].fun) (d_lastquiet);
}


/* routine to support the primitive expression Break */

void
ibreak()
{
  /* break returns a fault if debugging is off */
  if (!debugging_on || instartup) {
    apush(makefault("?debugging features not active"));
    return;
  }

  /* if we are already in a break, then d_step is on. 
     This tests makes Break have no additional effect */
  if (!d_onestep) {
    /* entr the fault break loop */
    break_loop(BREAKf);
  }
  apush(Nullexpr);
}



/* The routine break_loop provides a loop to accept a debug command 
   or requests for execution. It is called in two ways:

   flag == BREAKf called from ibreak()
      - initializes the break flags to stop in eval and returns

   flag == STEPf  step break
      - call from the expr_seq case in eval(). This is how step,
        next, and toend are controlled.
 */

static void
break_loop(int flag)
{
  char        newprompt[80];

#ifdef DEBUG
  nialptr     oldtop = topstack;
#endif
  newprompt[0] = (char) 0;
  debug_flags("break_loop");

  if (flag == BREAKf) {
    if (d_forceresume && d_repeatcount > 0) {
      d_repeatcount--;
      return;
    }
    d_forceresume = false;
    d_onestep = true;        /* this will cause us to stop in eval */
    if (d_repeatcount == 0) {
      d_lastcommand = STEPV_LOC;
      d_lastquiet = false;
    }
    nprintf(OF_NORMAL_LOG,
         "-------------------------------------------------------------\n");
    nprintf(OF_NORMAL_LOG,
            "    Break debug loop: enter debug commands, expressions or \n");
    nprintf(OF_NORMAL_LOG,
            "      type: resume    to exit debug loop\n");
#ifndef DEBUGNOENTER
    nprintf(OF_NORMAL_LOG,
            "      <Return> executes the indicated debug command\n");
#endif
    if (call_stack_pointer > 0) {
      nprintf(OF_NORMAL_LOG,
              "    current call stack :\n");
      dump_call_stack();
    }

    nprintf(OF_NORMAL_LOG,
         "-------------------------------------------------------------\n");
  }
  if (flag == STEPf) {
    d_infaultbreakloop = true;
    d_forceresume = false;
    /* The following will include the default command in the prompt */
#ifdef DEBUGNOENTER
    strcat(newprompt, "--> ");
#else
    strcat(newprompt, "-->[");
    strcat(newprompt, debugcommandlist[d_lastcommand].name);
    strcat(newprompt, "] ");
#endif

    /* loop to accept input until a debug command is executed */
    debug_flags("before while: ");
    while (d_infaultbreakloop && !d_forceresume) {
      if (d_repeatcount > 0) {
        debug_flags("repeatcount>0: ");
        executelastdebugcommand();
        d_repeatcount--;     /* decrease counter since it has been done once */
        break;
      }
      /* accept input from the user */
      writechars(STDOUT, newprompt, (nialint) strlen(newprompt), false);
      readinput();

#ifndef DEBUGNOENTER
      /* If the user presses enter, then the last debug command is issued
       * again, BUT without printing the resulting value. */
      if (tally(top) == 0) {
        freeup(apop());      /* the inputted text */
        d_infaultbreakloop = false; /* we don't want to go around here again */
        executelastdebugcommand();
      }
      else
#endif
      
      /* input was entered  */
          
      {
        int         cmdresult;
        if (*pfirstchar(top) == '\\') {
          /* escape character inplies the line is Nial text to execute */
          irest();           /* to remove escape */
          goto normal;
        }
        else
          /* check if have we found a debugging command and execute it if so */
          cmdresult = executeDebugCommand(pfirstchar(top));
        if (cmdresult > 0) {
          freeup(apop());
          d_repeatcount--;   /* decrease counter since it has been done once */
        }
        else if (cmdresult == 0) {  /* do normal evaluation of the user input */
          debug_flags_ptr sv_flags;

      normal:
          /* save current debug flags, and then reset */
          sv_flags = save_flags();
          reset_debug_flags();
          iexecute();
          if (top != Nullexpr) {
            ipicture();
            show(apop());
          }
          else
            freeup(apop());

          /* restore the proper debug flags */
          restore_flags(sv_flags);

#ifdef DEBUG
          if (topstack != oldtop) {
            while (topstack != oldtop)
              freeup(apop());
            exit_cover1("evaluation stack not empty",NC_FATAL);
          }
#endif
        }                    /* end of execute as Nial text */
      }                      /* end of input case */
    }                        /* end of while */
  }                          /* if (flag == STEPf) */
}


/* routines to manage the break list. This is a list of phrases
for the definitions in which breakin has been set. Its purpose is
to display to the user the current set of breaks. */



/* d_add_break adds a name to the list of definition names that have 
   the break flag set to true. Its argument is a phrase. */

static void
d_add_break(nialptr name)
{
  int         loc;

  /* search for the name in the list */
  for (loc = 0; loc < tally(breaklist); loc++) {
    if (name == fetch_array(breaklist, loc))
      break;
  }
  /* only if the name is NOT already in the list do we we want to add it */
  if (loc == tally(breaklist)) {
    decrrefcnt(breaklist);   /* to make it available for cleanup in append */
    append(breaklist, name);
    breaklist = apop();
    incrrefcnt(breaklist);   /* to protect it */
  }
}


/* routine to remove a name rorm the break list */

void
d_remove_break(nialptr name)
{
  nialint     loc,
              size;
  nialptr     newblist;

  /* search for the name in the list */
  for (loc = 0; loc < tally(breaklist); loc++) {
    if (name == fetch_array(breaklist, loc))
      break;
  }

  /* ignore if name is not found since DEFN calls this routine not 
     knowing if the name has to be removed */
  if (loc == tally(breaklist))
    return;

  /* create the new list */
  size = tally(breaklist) - 1;
  newblist = new_create_array(atype, 1, 0, &size);

  /* use 2 copies to fill it except for entry at loc */
  copy(newblist, 0, breaklist, 0, loc);
  copy(newblist, loc, breaklist, loc + 1, size - loc);

  /* reset the breaklist variable */
  decrrefcnt(breaklist);
  freeup(breaklist);
  breaklist = newblist;
  incrrefcnt(breaklist);
}


/* ibreakin implements the breakin primitive operation that allows
   debugging of individual named expressions and operations.
   This function is similar to isetdeftrace except that it also
   has to manage the breaklist. */

void
ibreakin()
{
  nialptr     x,
              sym,
              entr,
              nm,
              newnm,
              flg;
  nialint     res;
  int         test;

  x = apop();
  if (!debugging_on || instartup) {
    apush(makefault("?debugging features not active"));
    freeup(x);
    return;
  }

  if (tally(x) == 2) {       /* request to set the break flag to a given
                              * value (o or l); */
    splitfb(x, &nm, &flg);
    if (kind(nm) != chartype && kind(nm) != phrasetype) {
      apush(makefault("?invalid first argument in breakin"));
      freeup(x);
      return;
    }
    if (kind(flg) != booltype && kind(flg) != inttype) {
      apush(makefault("?invalid second argument in breakin"));
      freeup(x);
      return;
    }
    /* get set value */
    test = ((kind(flg) == booltype && boolval(flg)) ||
            (kind(flg) == inttype && intval(flg) == 1));
    apush(nm);               /* push name */
    freeup(x);               /* freeup argument pair, nm is protected */
  }
  else {                     /* assume x is the name and hence toggle the
                                setting */
    if (kind(x) != chartype && kind(x) != phrasetype) {
      apush(makefault("?invalid argument in breakin"));
      freeup(x);
      return;
    }
    apush(x);
    test = -1;               /* to indicate toggling */
  }

  newnm = getuppername();    /* convert name on stack to upper case */
  entr = lookup(newnm, &sym, globals);  /* look for it as global */
  if (entr == notfound) {
    apush(makefault("?undefined name in breakin"));
    freeup(newnm);
    return;
  }
  /* only opforms or expressions can be broken in */
  if (sym_role(entr) != Roptn && sym_role(entr) != Rexpr) {
error:
    apush(makefault("?can break only in a named opform or expression"));
    freeup(newnm);
    return;
  }
  /* now check that an op is an opform */
  if (sym_role(entr) == Roptn && tag(sym_valu(entr)) != t_opform)
    goto error;

  res = sym_brflg(entr);     /* get the break flag value to return */

  /* set test if we are toggling */
  if (test == -1)
    test = !res;

  /* set flag to the value of test and adjust break list */
  st_s_brflg(entr, test);
  if (test)
    d_add_break(newnm);
  else
    d_remove_break(newnm);
  apush(createbool(res));
  freeup(newnm);
}


/* routine to implement the expression Breaklist */

void
ibreaklist()
{
  if (!debugging_on) {
    apush(makefault("?debugging features not active"));
  }
  else
    apush(breaklist);
}

/* routines to manage the watch list */

/* The watch list is a list of global or scoped variables
   and actions that are kept to assist debugging. 

   The entries in the watch list are triples consisting of:
   the symbol table number, the entry for the variable relative to
   that symbol table and the action string.

   Because symbol tables and entry nos are unique the comparisons
   in searching the watchlist can be done on nialptr equality. */



/* routine to add a name to the watch list. */

static nialptr
d_add_watch(nialptr sym, nialptr entr, nialptr action)
{
  nialint     loc,
              three = 3;
  nialptr     witem = Null,
              newitem = new_create_array(atype, 1, 0, &three);

  /* fill new item for watch list */
  store_array(newitem, 0, sym);
  store_array(newitem, 1, entr);
  store_array(newitem, 2, action);

  /* look to see if an action already exists for this variable */
  for (loc = 0; loc < tally(watchlist); loc++) {
    witem = fetch_array(watchlist, loc);
    if (fetch_array(witem, 0) == sym && fetch_array(witem, 1) == entr)
      break;
  }

  if (loc == tally(watchlist)) {  /* entry not found */
    decrrefcnt(watchlist);   /* prepare for replacing */
    append(watchlist, newitem); /* extend the watchlist to a new one */
    watchlist = apop();
    incrrefcnt(watchlist); /* protect it again */
    return Null;
  }
  else {                     /* witem holds the old entry */
    apush(witem);            /* to protect the old item */
    replace_array(watchlist, loc, newitem);
    return apop();
  }
}

/* routine to remove a specific watch from the watch list */

static      nialptr
d_remove_watch(nialptr sym, nialptr entr)
{
  nialint     loc,
              size;
  nialptr     witem = Null,
              newwlist;

  /* find where the watch is in the list */

  for (loc = 0; loc < tally(watchlist); loc++) {
    witem = fetch_array(watchlist, loc);
    if (fetch_array(witem, 0) == sym && fetch_array(witem, 1) == entr)
      break;
  }

  /* if the watch is not found leave watchlist unchanged */
  if (loc == tally(watchlist))
    return Null;

  /* create the new list */

  size = tally(watchlist) - 1;
  newwlist = new_create_array(atype, 1, 0, &size);

  /* use 2 copies to fill it except for entry at loc */

  copy(newwlist, 0, watchlist, 0, loc);
  copy(newwlist, loc, watchlist, loc + 1, size - loc);

  apush(witem);              /* to protect it */
  decrrefcnt(watchlist);
  freeup(watchlist);
  watchlist = newwlist;
  incrrefcnt(watchlist);
  return apop();
}


/* routine to clear all variable watches for a particular local symtab */

void
d_clear_watches(nialptr sym, nialptr entr)
{
  int         cnt = 0,
              i;

  /* push the watch items not in this symbol table and count them */
  for (i = 0; i < tally(watchlist); i++) {
    nialptr     witem = fetch_array(watchlist, i);

    if (sym == global_symtab) { /* only clear the one variable */
      if (fetch_array(witem, 1) != entr) {
        apush(witem);
        cnt++;
      }
    }
    else
     /* clear for all variables with this symbol table */ 
    if (fetch_array(witem, 0) != sym) {
      apush(witem);
      cnt++;
    }
  }
  /* make the new watchlist on the stack */
  mklist(cnt);

  /* cleanup old list */
  decrrefcnt(watchlist);
  freeup(watchlist);

  /* store the new list and protect it */
  watchlist = apop();
  incrrefcnt(watchlist);
}


/* routine to implement iwatchlist. It returns a list of pairs
   giving the symbolic reference for the variables and the actions. */

void
iwatchlist()
{
  if (!debugging_on) {
    apush(makefault("?debugging features not active"));
  }
  else {
    char       *var_name,
                xxx[256];
    int         i;

    /* construct the pair for each item of the watchlist and push it on the
      stack */
    for (i = 0; i < tally(watchlist); i++) {
      nialptr     witem = fetch_array(watchlist, i),
                  sym = fetch_array(witem, 0),
                  entr = fetch_array(witem, 1),
                  action = fetch_array(witem, 2);

      var_name = pfirstchar(sym_name(entr));  /* safe: no allocation */
      if (sym == global_symtab)
        sprintf(xxx, "!%s", var_name);
      else {
        char       *dfn_name = pfirstchar(get_symtabname(sym));  
                       /* safe: no allocation */

        sprintf(xxx, "!%s:%s", dfn_name, var_name);
      }

      pair(makephrase(xxx), action);
    }
    mklist(tally(watchlist));
  }
}


/* routine to look for a watch on a variable and do its action */

static void
do_watch_action(nialptr sym, nialptr entr)
{
  int         i;

  /* search the watch list for a matching entry for this symbol table */
  for (i = 0; i < tally(watchlist); i++) {
    nialptr     witem = fetch_array(watchlist, i);

    if (fetch_array(witem, 0) == sym && fetch_array(witem, 1) == entr) {  
      /* entry found. do its action */
      nialptr     action = fetch_array(witem, 2);

      if (tally(action) != 0) {
        apush(action);
        iexecute();
        if ((kind(top) == faulttype) && (top != Nullexpr)) {  
          /* fault in watch action */
          char        ermsg[256];
          nialptr     flt = apop();

          sprintf(ermsg,
                "---> Watch on \"%s\" in scope \"%s\", produced fault: %s ",
                  pfirstchar(sym_name(entr)), pfirstchar(get_symtabname(sym)),
                  pfirstchar(flt));
          mkstring(ermsg);
          iwrite();
        }
        freeup(apop());
      }
      return;                /* return after first match */
    }
  }                          /* end for */
}



/* routine to implement primitive operation watch, which adds a watch to the list.
   The argument to watch is a variable reference and a string
   indicating the action to be taken. The variable reference is
   given as !<var> for a global variable, or as !<dfn>:<var>
   for a scoped variable.

   The routine must resolve the reference into a symbol table and
   an entry, and use d_add_watch to add it to the list. The
   result from the add routine is the argument modified to have the
   previous action.
   */

void
iwatch()
{
  nialptr     z = apop();    /* grab the pair (parse tree, string) */
  nialptr     action,
              ptree,
              body,
              res,
              oldwatch;
  nialptr     dfn_sym = Null,
              var_entr = Null;

  if (!debugging_on) {
    apush(makefault("?debugging features not active"));
    freeup(z);
    return;
  }

  /* Check arguments for type and tally */
  if ((tally(z) != 2) || (kind(z) != atype)) {
    apush(makefault("?Watch requires a parse tree and a string or phrase"));
    freeup(z);
    return;
  }

  action = fetch_array(z, 1);/* grab the string to execute  */
  ptree = fetch_array(z, 0); /* grab the parse tree */

  /* arg check the parse tree */
  if ((kind(ptree) != atype) || (tally(ptree) == 0))
    if (ptag(ptree) != t_parsetree) {
      apush(makefault("?first argument of watch must be a parse tree"));
      freeup(z);
      return;
    }

  /* arg check the string argument */
  if (!istext(action)) {
    apush(makefault("?second argument of watch must be a string or phrase"));
    freeup(z);
    return;
  }


  body = pbody(ptree);       /* pull out the body of the parse tree */

  /* parse tree must be a variable or a scoped var xxx:ggg */
  if (!((tag(body) == t_variable) || (tag(body) == t_scoped_var))) {
    buildfault("Parse tree must be for a variable");
    freeup(z);
    return;
  }

  /* If we have a variable parse tree */
  if (tag(body) == t_variable) {
    dfn_sym = get_sym(body); /* the symbol tbl the var is in */
    if (dfn_sym != global_symtab) {
      apush(makefault("?cannot add watch to a local variable without scope"));
      freeup(z);
      return;
    }

    var_entr = get_entry(body); /* the entry for the var */

  }
  else 
  if (tag(body) == t_scoped_var) { /* otherwise it's a scoped variable */
    nialptr     varphrase,
                dfn_tree = get_idfun(body), /* the id for the defn */
                dfn_id = get_entry(dfn_tree), /* the entry for the defn id */
                dfn_body = sym_valu(dfn_id);  /* the defn's parse tree body */

    if (tag(dfn_body) == t_expression || tag(dfn_body) == t_opform ||
        tag(dfn_body) == t_trform) {  /* the defn has a scope */
      dfn_sym = get_trsym(sym_valu(dfn_id));  /* the symtab for the defn */

      varphrase = get_name(get_idvar(body));  /* the name for id being sought */
      var_entr = Blookup(get_root(dfn_sym), varphrase); /* entry for id */

      if (var_entr == notfound) { /* id is not in the scope */
        apush(makefault("?scoped variable not found in watch"));
        freeup(z);
        return;
      }
    }
    else {
      apush(makefault("?not a definition with scope in watch"));
      freeup(z);
      return;
    }

  }

  /* add the new watch. */

  /* if the action is empty, then we are clearing the watch */
  if (tally(action) == 0)
    oldwatch = d_remove_watch(dfn_sym, var_entr);
  else
    oldwatch = d_add_watch(dfn_sym, var_entr, action);

  /* make the return value */
  res = new_create_array(atype, 1, 0, shpptr(z, 1));
  store_array(res, 0, fetch_array(z, 0));
  if (oldwatch == Null) {
    store_array(res, 1, Null);
  }
  else {
    store_array(res, 1, fetch_array(oldwatch, 2));
  }
  apush(res);
  freeup(z);
  freeup(oldwatch);
}



/* routine to support the operation seeusercalls which toggles
   the flag to record user call information */

void
iseeusercalls()
{
  nialptr     x = apop();
  int         oldstatus = userroutines;

  if (!debugging_on) {
    apush(makefault("?debugging features not active"));
    freeup(x);
    return;
  }
  if (atomic(x) && kind(x) == booltype)
    usercallvalue = boolval(x);
  else if (atomic(x) && kind(x) == inttype)
    usercallvalue = intval(x);
  else {
    apush(makefault("?seeusercalls expects a truth-value"));
    freeup(x);
    return;
  }
  usercallchanged = true;
  apush(createbool(oldstatus));
}

/* routine to support the operation seeprimcalls which toggles
   the flag to record primitive call information */


void
iseeprimcalls()
{
  nialptr     x = apop();
  int         oldstatus = primroutines;

  if (!debugging_on) {
    apush(makefault("?debugging features not active"));
    freeup(x);
    return;
  }
  if (atomic(x) && kind(x) == booltype)
    primcallvalue = boolval(x);
  else if (atomic(x) && kind(x) == inttype)
    primcallvalue = intval(x);
  else {
    apush(makefault("?seeprimcalls expects a truth-value"));
    freeup(x);
    return;
  }
  primcallchanged = true;
  apush(createbool(oldstatus));
}


/* routine to display the call stack. It is called when a fault triggers
   a jump to top level and from icallstack(). */

#define CALLSTACKDISPLAYLIMIT 10
static void
dump_call_stack()
{
  int         cnt, start = 0;
  if (call_stack_pointer >= CALLSTACKDISPLAYLIMIT)
      start = call_stack_pointer - CALLSTACKDISPLAYLIMIT;
  for (cnt = start; cnt < call_stack_pointer; cnt++) {
    char       *tmp_str = slower(pfirstchar(sym_name(call_stack[cnt])));

    nprintf(OF_NORMAL_LOG, "%s\n", tmp_str);
    free(tmp_str);
  }
}


/* routine called to add entry to call stack */

static void
enter_op(nialptr entr)
{
  /* allocate the call stack if needed */
  if (!call_stack_allocated) {
    call_stack = (nialptr *) malloc(call_stack_size * sizeof(nialptr));
    if (call_stack == NULL)
      exit_cover1("Not enough memeory to continue", NC_FATAL);
    call_stack_allocated = true;
  }

  /* increase call_stack size if needed */
  if (call_stack_pointer + 1 == call_stack_size) {
    nialptr    *newst;

    call_stack_size += CALL_STACK_SIZE_INC;
    nprintf(OF_MESSAGE, "increasing call stack to %d\n", call_stack_size);
    newst = (nialptr *) realloc((void *) call_stack,
                                call_stack_size * sizeof(nialptr));
    if (newst == NULL) {
      exit_cover1("Not enough memeory to continue", NC_FATAL);
    }
    else
      call_stack = newst;
  }

  /* store the call_stack entry */

  call_stack[call_stack_pointer] = entr;

  if (debugging_on) {
    if (userroutines) {
      char       *tmp_str = slower(pfirstchar(sym_name(entr)));

      nprintf(OF_NORMAL, "entering %s\n", tmp_str);
      free(tmp_str);
    }
  }
  call_stack_pointer++;
}

/* routine to remove an entry form the call stack */

static void
exit_op()
{
  if (call_stack_pointer > 0) { /* test so that it never goes negative */
    call_stack_pointer--;
    if (debugging_on) {
      if (userroutines) {
        nialptr     entr = call_stack[call_stack_pointer];
        char       *tmp_str = slower(pfirstchar(sym_name(entr)));

        nprintf(OF_NORMAL, "leaving %s\n", tmp_str);
        free(tmp_str);
      }
    }
  }
}

/* routine to support the primitive expression Callstack.
  Because the op call stack will be cleaned up on it's own
  this function does not free up the stack entries, nor does
  it reset the stack size to 0. */

void
icallstack()
{
  if (call_stack_pointer) {
    nprintf(OF_NORMAL_LOG, "Current Callstack is :\n");
    dump_call_stack();
  }
  else
    nprintf(OF_NORMAL_LOG, "Callstack empty\n");
  apush(Nullexpr);
}

void
clear_call_stack()
{
  call_stack_pointer = 0;
}

void
destroy_call_stack()
{
  clear_call_stack();
  if (call_stack) {
    free(call_stack);
    call_stack = NULL;
    call_stack_allocated = 0;
  }
}

/*  end of routines to handle the watches and callstack  */

/* routines to do fault break loop. Called in absmach.c */

void
process_faultbreak(char *f)
{
  /* only do a break if we are NOT at the top level and not breaking already */
  if (call_stack_pointer != 0 && !instartup) {

    if (nct_in_catch_throw()) {
      /* This will not return */
      nct_throw_result(createatom(faulttype, f), 1);
    }

    triggered = false;
    fault_loop(createatom(faulttype, f));
    triggered = true;
    longjmp(error_env, NC_FAULT); /* ws_cleanup called in coreif.c */
  }
}


/* fault_loop provides a loop to accept requests for execution when
    a fault interrupt occurs. It only accepts Nial expressions for evaluation.
*/

static void
fault_loop(nialptr triggeredfault)
{
  char        newprompt[80];

#ifdef DEBUG
  nialptr     oldtop = topstack;
#endif

  newprompt[0] = (char) 0;

  /* display fault loop information */
  nprintf(OF_NORMAL_LOG,
          "-------------------------------------------------------------\n");
  nprintf(OF_NORMAL_LOG,
          "    Fault interruption loop:  enter expressions or \n");
  nprintf(OF_NORMAL_LOG,
          "      type: <Return>   to jump to top level\n");
  if (call_stack_pointer > 0) {
    nprintf(OF_NORMAL_LOG,
            "    current call stack :\n");
    dump_call_stack();
  }

  nprintf(OF_NORMAL_LOG,
          "      %s triggered in : ", pfirstchar(triggeredfault));

  showexpr(saveexp, TRACE);

  nprintf(OF_NORMAL_LOG,
          "-------------------------------------------------------------\n");


  /* set the prompt */
  strcat(newprompt, ">>> ");

  /* loop to accept input to execute until an empty line */
  while (true) {             
    /* accept input from the user */
    writechars(STDOUT, newprompt, (nialint) strlen(newprompt), false);
    readinput();

    if (tally(top) == 0)     /* end the loop if input is empty */
        break;
    iexecute();
    if (top != Nullexpr) {
      ipicture();
      show(apop());
    }
    else
      freeup(apop());

#ifdef DEBUG
    if (topstack != oldtop) {
      nprintf(OF_DEBUG, "system error: stack not empty in fault loop\n");
      while (topstack != oldtop)
        freeup(apop());
      exit_cover1("stack not empty in fault loop",NC_FATAL);
    }
#endif

  }                          /* end of while */
}


/* varname is a routine to pick up a name for ivalue, iassign, iupdate,
   or iapply. It is used in here ineval.c and in insel.c and mainlp.c.
   The arg may be a character string, a phrase or a parse tree built by ! or 
   by the user.
   It returns false if not a name, or notfound, except if dynamicassign is on,
   then an entry is created if needed.(ie it only fails if nm is not
   character data.)
   The parameter applyflg indicates that an operation is being looked for,
   and, if found, the operation definition is returned in sym instead of
   a sym,entry pair.
   The parameter dynamicassign indicates it is a call from "assign"
   or from the history mechanism in mainlp.c.
   It returns true if the nm is valid; the calling
   routine must determine if entry is the right type for its application.
*/

int
varname(nialptr nm, nialptr * sym, nialptr * entr, int applyflg, int dynamicassign)
{
  nialptr     upnm;

  if (tally(nm) == 0)
    return (false);
  if (kind(nm) == atype) {
    if (tag(nm) == t_parsetree) {
      nm = pbody(nm);
      if (tag(nm) == t_parendobj || tag(nm) == t_dottedobj)
        nm = get_obj(nm);
      if (tag(nm) != t_variable)
      { if (applyflg && ( tag(nm) == t_composition || tag(nm) == t_curried || 
             tag(nm) == t_opform || tag(nm) == t_transform || 
             tag(nm) == t_basic || tag(nm) == t_atlas)) {
          *sym = nm;
          return (true);
        }
        else
          return (false);
      }
      if (applyflg) 
      { if (get_role(nm) == Roptn)
          *sym = nm;
        else
          return (false);
      }
      else 
      { *sym = get_sym(nm);
        *entr = get_entry(nm);
      }
      return (true);
    }
    else
      return (false);
  }
  else {
    apush(nm);               /* protect nm in getuppername */
    apush(nm);
    upnm = getuppername();
    apop();                  /* to unprotect nm */
    if (upnm != grounded) {  /* grounded used as flag that it is not a name */
      *entr = lookup(upnm, sym, passive); /* always a passive search */
      if (applyflg) {        /* looking for an operation */
        if (*entr != notfound && sym_role(*entr) == Roptn) {
          /* the optn is returned in sym */
          *sym = fetch_var(*sym, *entr);
          return (true);
        }
        else {
          freeup(upnm);
          return (false);
        }                    /* done here for iapply */
      }
      if (dynamicassign) {   /* assignment to lhs */
        if (*entr == notfound) {
          *entr = mkSymtabEntry(global_symtab, upnm, Rvar, no_value, false);
          *sym = global_symtab;
        }
      }
      freeup(upnm);
      return (*entr != notfound);
    }
    else
      return (false);
  }
}

/* routine to implement the primitive operation "value" 
   that looks up the value of a variable. The argument can be a string,
   a phrase, or the cast of a variable name. */

void
ivalue(void)
{
  nialptr     nm,
              entr,
              sym;

  nm = apop();
  if (varname(nm, &sym, &entr, false, false)) {
    if (sym_role(entr) == Rvar || sym_role(entr) == Rconst) {
      apush(fetch_var(sym, entr));
    }
    else
      buildfault("invalid name");
  }
  else
    buildfault("invalid name");
  freeup(nm);
}

/* routine to implement the primitive operation "assign" that
   assigns the second argument to the variable represented by the first
   argument. The first argument can be a string, a phrase, or the cast of 
   a variable name. */

void
iassign(void)
{
  nialptr     val,
              x,
              idlist,
              role,
              entr,
              sym;

  x = apop();
  if (kind(x) == atype) {
    if (tally(x) == 2) {
      /* check validity and get symbol table and entry */
      if (varname(fetch_array(x, 0), &sym, &entr, false, true)) {
        role = sym_role(entr);
        if (role == Rvar || role == Rident) {
          /* assingment is valid. get value to assign */
          val = fetch_array(x, 1);
          /* if a new identifier set role to variable */
          if (role == Rident)
            st_s_role(entr, Rvar);
          /* set up temporary id list holding the one variable */
          idlist = nial_extend(st_idlist(), b_variable((nialint) sym, (nialint) entr));
          /* do the assignment and push the value as the result */
          assign(idlist, val, false, true);
          apush(val);
          freeup(idlist);  /* free temporary id list */
        }
        else
          buildfault("not a variable");
      }
      else
        buildfault("invalid name");
    }
    else
      buildfault("invalid assign");
  }
  else
    buildfault("invalid assign");
  freeup(x);
}


/* routine to implement the primitive operation "apply" that
   applies the first argument to the second one. The first argument
   can be a string, a phrase, or the cast of an operation name or 
   operation expression */

void
iapply()
{
  nialptr     z,
              nm,
              arg,
              optn,
              dummy;

#ifdef DEBUG
  if (debug)
    memchk();
#endif
  z = apop();
  if (tally(z) == 2) {
    splitfb(z, &nm, &arg);
    /* validate that nm is an operation and get code in optn */
    if (varname(nm, &optn, &dummy, true, false)) {
      apush(arg);
      apply(optn);    
    }
    else {
      apush(makefault("?not an operation"));
      freeup(arg);
    }
    freeup(nm);              /* in case z is homogeneous */
  }
  else
    apush(makefault("?apply requires a pair"));
  freeup(z);
}


/* --------------- Support routines for catch/throw ---- */


extern void pair(nialptr x, nialptr y);
extern void iapply(void);
extern void nct_do_eval(nialptr exp);
extern void apply(nialptr fn);

extern int get_current_call_stack();
extern void reset_current_call_stack(int old_stack);


/** This holds the currently active jump buffer */
jmp_buf *ct_current_jmp_buf = NULL;

void icatch(void) {
    nialptr f, x;
    jmp_buf catch_buf, *current_catch_buf;
    nialint arg_stack;
    int call_stack;
    int rc;
    
    /* Get the function argument */
    f = apop();
    
    /* Get the data argument */
    x = apop();
    
    /* Add another longjmp sequence */
    current_catch_buf = ct_current_jmp_buf;
    ct_current_jmp_buf = &catch_buf;
    
    /* Save the stack poistions for rewind */
    arg_stack = topstack;
    call_stack = get_current_call_stack();
    
    /* Set up the jump_buf */
    rc = setjmp(catch_buf);
    if (rc) {
        /* Entered via a long jump */
        nialptr res = apop();        /* get result off stack */
        
        ct_current_jmp_buf = current_catch_buf;      /* unwind jmpbuf */
        reset_current_call_stack(call_stack);   /* unwind call stack */
        
        while (arg_stack != topstack) {         /* unwind arg stack */
            freeup(apop());
        }
        
        apush(res);
        return;
    }
    
    /* Put x back on the stack and call apply */
    apush(x);
    apply(f);
    
    /* if we return here there were no throws */
    ct_current_jmp_buf = current_catch_buf;      /* unwind jmpbuf */
    return;
}


/**
 * Function to throw a result value.
 * This should only be called if we are in a catch/throw
 * sequence.
 *
 * The rc_code argument is for the future but at
 * moment should always be 1.
 *
 * Used by eval.c as wel as below.
 */

void nct_throw_result(nialptr res, int rc_code) {
    apush(res);
    longjmp(*ct_current_jmp_buf, rc_code);
    return;
}


/**
 * primitive to throw a result value. If we are not in a
 * catch/throw sequence it returns a fault.
 */
void ithrow(void) {
    if (ct_current_jmp_buf == NULL) {
        nialptr x = apop();
        apush(makefault("?no_catch"));
        freeup(x);
    } else {
        nct_throw_result(apop(), 1);
    }
    return;
}



/**
 * Return true if we are in a catch/throw sequence
 This is use by eval.c
 */
int nct_in_catch_throw() {
    return (ct_current_jmp_buf != NULL);
}

/**
 * Returns the current call stack for recovery
 */
int get_current_call_stack() { return call_stack_pointer; }


/**
 * Recovery of call stack to earlier position
 */ 
void reset_current_call_stack(int old_stack) {
  while(call_stack_pointer != old_stack) {
    nialptr ent = call_stack[--call_stack_pointer];
    freeup(ent);
  }
}


/**
 * Call into eval fo catch/throw
 */
void nct_do_eval(nialptr x) {
  eval(x);
  return;
}


