/*==============================================================

  EVAL.H:  HEADER FOR EVAL.C and EVAL_FUN.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the prototypes of the evaluation functions

================================================================*/


#ifndef _EVAL_H_
#define _EVAL_H_


/* If C level debugging is on eval calls d_eval and n_eval internally */

#ifndef DEBUG
extern void d_eval(nialptr exp);
extern void n_eval(nialptr exp);
#else
extern void eval(nialptr exp);
#endif

extern void apply(nialptr fn);
extern int  assign(nialptr varlist, nialptr val, int trsw, int valneeded);
extern nialptr fetch_var(nialptr sym, nialptr entr);
extern int  store_var(nialptr sym, nialptr entr, nialptr v);
extern void coerceop(void);
extern int  b_closure(void);
extern void clear_call_stack(void);



extern void applyprimitive(nialptr p);
extern void applybinaryprim(nialptr p);

#define APPLYPRIMITIVE(p) (debugging_on?applyprimitive(p):(*applytab[get_index(p)])())
#define APPLYBINARYPRIM(p) (debugging_on?applybinaryprim(p):(*binapplytab[get_binindex(p)])())


#ifndef DEBUG
/* define eval to switch between versions with and without debugging */
#define eval(exp)   (debugging_on ? d_eval(exp) : n_eval(exp))
#endif

/* do_apply avoids recursive call to apply() for a basic operation */
#define do_apply(p) (tag(p)==t_basic?(*applytab[get_index(p)])():apply(p))



/* value of symbol table pointer from a symbol table reference.
  Only used with eval.c and eval_fun.c */

#define spval(sym) intval(*(pfirstitem(sym)+1))

/* stuff from debug.h */


#undef DEBUGNOENTER
/*#define DEBUGNOENTER */
#define DEBUGLINES 2

struct dfs {
  int         d_quiet;
  int         d_forceresume;
  int         d_infaultbreakloop;
  int         d_onestep;
  int         d_toend;
  int         d_next;
  int         d_stepin;
  int         d_trace;
  int         d_lastcommand;
  int         d_lastquiet;
  int         d_repeatcount;
};


/* used to pass prompttype arg to showexpr */
enum {
  QUESTION, TRACE
};

typedef struct dfs debug_flags_str,
           *debug_flags_ptr;



enum {
  BREAKf, FAULTf, STEPf
};


#define STEPV_LOC 2
#define TOEND_LOC 3
#define RESUME_LOC 5

/* To see debug flags, switch the define with the undef below so that 
   DEBUG_PRINT is defined.
*/
#undef DEBUG_PRINT
#ifdef DEBUG_PRINT
#define debug_flags(id) nprintf(OF_NORMAL,"%s:te=%d,os=%d,fr=%d,fbl=%d,n=%d,\
si=%d tr=%d\n",id,d_toend,d_onestep,\
					 d_forceresume,d_infaultbreakloop,d_next,\
					 d_stepin,trace)
#else

#define debug_flags(id)      /* */
#endif

#define   CALL_STACK_SIZE_INC 100

extern void clear_call_stack(void);
extern void destroy_call_stack(void);
extern void init_debug_flags(void);
extern void d_clear_watches(nialptr, nialptr);
extern void d_remove_break(nialptr);

extern void process_faultbreak(char *f);
extern int  varname(nialptr nm, nialptr * sym, nialptr * entr, int applyflg, int dynamicassign);

#endif
