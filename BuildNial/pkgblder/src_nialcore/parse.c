/* ==============================================================

   MODULE     PARSE.C

  COPYRIGHT NIAL Systems Limited  1983-2016


  Contains the routines to parse Nial code fragments and store them
  as parse trees represented as nested arrays.

================================================================*/


 /* The Nial parser is implemented as a hybrid consisting of a top down recursive
    descent parser for the control structure mechanisms and a bottom up shift-reduce
    parser for the expression sub-language.  Mike Jenkins wrote the recursive descent 
    routines and Carl McCroskey did the shift reduce parser.

    The latter part permits back up to handle certain look ahead cases. This is done
    in a non-standard way to avoid exponential time to parse long strands.

  The parser supports both the syntax of Nial V4 with full tranformer expressions
  and the use of the syntactic dot, and that of Nial V6 that omits these features.

 */



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

/* SJLIB */
#include <setjmp.h>

/* Q'Nial header files */

#include "parse.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"

#include "eval.h"            /* for eval, d_clear_watches */
#include "token.h"           /* for token properties */
#include "reserved.h"        /* for reserved word macros */
#include "symtab.h"          /* for symtab macros and lookup */
#include "blders.h"          /* for builders */
#include "getters.h"         /* for getters */
#include "roles.h"           /* for role codes */
#include "fileio.h"          /* for nprintf */
#include "ops.h"             /* for append hitch */
#include "if.h"              /* for checksignal */



static void accept1(void);
static void resettkn(nialint n);
static nialint  DEFN(nialptr * tree, int inblock);
static nialint  ACTION(nialptr * tree);
static nialint  EXPRESSION(nialptr * tree);
static nialint  PRIMARY(nialptr * tree);
static nialint  EXPR_SEQ(nialptr * tree);
static nialint  STMT_EXPR(nialptr * tree);
static nialint  COMMENT(nialptr * tree);
static void IDLIST(nialptr * tree, int searchtype);
static nialint  ID(nialptr * tree, int searchtype);
static void backup(void);
static void pushexpr(int st, nialptr tr);
static void startexprstack(void);
static void cleanupexprstack(void);
static void extendexprstack(void);
static void additem(int state);
static void finishcollection(void);
static nialptr popexpr(void);
static void parentop(void);
static nialptr varinstall(nialptr id, int role, nialptr sym);
static void chopstack(void);
static nialint  Xpp(void);
static nialint  Xp_(void);
static nialint  Xsp(void);
static nialint  Xs_(void);
static nialint  Xao(void);
static nialint  XaRE(void);
static nialint  Xoa(void);
static nialint  Xoo(void);
static nialint  XoRE(void);
static nialint  XocRE(void);
static nialint  Xocp(void);
static nialint  Xoco(void);
static nialint  Xoct(void);
static nialint  Xto(void);
static nialint  Xtco(void);
static nialint  OPERATION(nialptr * tree);
static nialint  TR_FORM(nialptr * tree);
static nialint  Xcmb(int t, nialptr(*b) (nialptr t1, nialptr t2));
static nialint  XcmbR(int t, nialptr(*b) (nialptr t1, nialptr t2));
static nialint  XxRE(int newform);
static nialint  XLERE(void);
static nialint  Xn_(void);
static nialint  X_n(void);
static nialint  XnRE(void);
static nialint  cant(void);
static nialint  quotepgm(void);
static nialint  rec_form(int exprflag);
static nialint  brackets(void);
static nialptr deref_sys(nialptr tree);
static nialint  shift(void);
static nialint  formfinder(nialptr * tree, int *k);
static nialint  OP_FORM(nialptr * tree);
static nialint  DEFN_SEQ(nialptr * tree, int inblock);
static nialint  BLOCK(nialptr * tree);
static nialint  BLOCKBODY(nialptr sym, nialptr args, nialptr * block);


static nialint  IX_VAR(nialptr * tree);
static nialint  SCOPED_VAR(nialptr * tree);
static nialint  IX_ASSIGN_EXPR(nialptr * tree);
static nialint  ASSIGN_EXPR(nialptr * tree);
static nialint  WHILE_EXPR(nialptr * tree);
static nialint  EXIT_EXPR(nialptr * tree);
static nialint  REPEAT_EXPR(nialptr * tree);
static nialint  FOR_EXPR(nialptr * tree);
static nialint  IF_EXPR(nialptr * tree);
static void ite_cleanup(int cnt);
static nialint  CASE_EXPR(nialptr * tree);

/* the DEBUG version of error shows an error no so we can determine which call
   to error produced it, while the production version omits it.
*/

#ifdef DEBUG
static void error(int messno, int errcnt);
#else
static void proderror(int messno);
#endif

/* globals just for parser routines */

static nialint localcnt,     /* keeps track of number of local names used in
                                a local scope. Used to set offset number for
                                activation records */
            loopcount;       /* counts the number of local loops open at one
                                time. Used in parsing loop constructs correctly. */

static int  token_index,     /* points to position of next token in the stream */
            tokenprop,       /* property of next token */
            tokentally;      /* number of token sin the stream */

static int  deferredsi,      /* token_index where deferred error occurred */
            lasterror;       /* errno in last call on error() */

#ifdef DEBUG
static int  lasterrcnt;      /* errcnt in last call on error() */
#endif

static nialptr nexttoken,    /* the next token to be examined */
            referred,        /* the list of names already referred to in the block */
            tokens;          /* the list of tokens */



/*  Stack State Values */

#define  s_any -1
#define  s_P   0
#define  s_S   1
#define  s_A   2
#define  s_O   3
#define  s_OC  4
#define  s_T   5
#define  s_TC  6
#define  s_LE  7
#define  s_RE  8
#define  s_N   9


/* routine to accept one token. It places the token property in tokenprop
   and the token string in nexttoken. If we are at the end of the token stream
   it sets nexttoken to r_EOL.
*/

static void
accept1()
{
  token_index += 2;
  if (token_index == tokentally) {
    nexttoken = r_EOL;
    tokenprop = delimprop;
  }
  else {
    tokenprop = intval(fetch_array(tokens, token_index));
    nexttoken = fetch_array(tokens, token_index + 1);
    if (tokenprop >= eolprop && tokenprop < constprop)
      accept1();
  }
}

/* routine to reset the token index to position n and reset the global
   variables tokenprop and nexttoken to reflect this.
*/

static void
resettkn(nialint n)
{
  if (token_index != n) {
    token_index = n;
    if (token_index == tokentally) {
      nexttoken = r_EOL;
      tokenprop = delimprop;
    }
    else {
      tokenprop = intval(fetch_array(tokens, token_index));
      nexttoken = fetch_array(tokens, token_index + 1);
    }
  }
}


/* Nial level interface to the parser.
   The parameter is false so that it can parse any expression.
*/

void
iparse()
{
  parse(false);
}


/* the parser entry routine. The parameter indicates whether it is
   to parse actions only, or any type of expression.
   The token stream to be parsed is on the evaluation stack.
   The parser takes no action if it is a fault.
*/

void
parse(int act_only)
{
  nialptr     tree;
  int         res,
              sav_loopcnt,
              k;

  if (top == Nullexpr || kind(top) == faulttype) 
    return;

  startexprstack();  /* start the internal parser stack for expressions. */

  tokens = apop();  /* pop the token stream into tokens */
  nonlocs = Null;
  referred = Null;
  sav_loopcnt = loopcount;
  loopcount = 0;

  if (tally(tokens) != 0 && tag(tokens) == t_tokenstream) {
    if (tally(tokens) == 1) {/* empty token stream */
      apush(b_parsetree(Nulltree));
    }
    else if (kind(tokens) == atype) {
      tokentally = tally(tokens);
      token_index = 0;
      resettkn(1);           /* arg must differ from setting of token_index
                                in order to initialize the token variables.
                                It is 1 to get past the token stream indicator */
      localcnt = 0;

      res = ACTION(&tree); /* attempt to parse an action */
      
      switch (res) {
        case SUCCEED:
            if (nexttoken == r_EOL) { /* entire token stream used */
              apush(b_parsetree(tree));  /* build the result tree and push it */
            }
            else { /* tokens left over */
              error(incomplete_parse, 1); /* 1 */
              freeup(tree);
            }
            break;
        case ERROR:
            break;
        case FAIL:    
            if (!act_only) { /* try formfinder and see if its an op or tr */
              resettkn(1);   /* backup to first token */

              res = formfinder(&tree, &k); /* attempt to parse a general expression */

              switch (res) {
                case SUCCEED:
                    if (nexttoken == r_EOL) { /* entire token stream used */
                      apush(b_parsetree(tree)); /* build the result tree and push it */
                    }
                    else {
                      error(incomplete_parse, 2);  /* 2 */
                      freeup(tree);
                    }       
                    break;
                case ERROR:
                    break;
                case FAIL:
#ifdef DEBUG
                    nprintf(OF_DEBUG, "****** parser logic error ******\n");
                    nabort(NC_ABORT);
#else
                    error(parser_error, 3); /* 3 */
#endif
              }
            }
            else
              error(parse_fail, 4); /* 4 */
      }
    }
    else
      buildfault("invalid type in parse");
  }
  else
    buildfault("parse needs token stream as argument");
  freeup(tokens);
  freeup(referred);
  freeup(nonlocs);
  loopcount = sav_loopcnt;
  cleanupexprstack();        /* remove expression stack */
}


/*
   ROUTINE NAME : ACTION

   parses an action.

   action       --|---definition_seq---->---|-------------->
   |                         |   -----
   |                         |->-| ; |------>
   |                             -----
   |---expr_seq----------------------------->
   |
   |                    -----
   |--definition_seq -->| ; |---expr_seq--->
                        -----

ACTION is called by parse() and quotepgm().

*/


static nialint
ACTION(nialptr * tree)
{
  int         res;

  /* look ahead to see if it is a definition. The test assumes that a
     definition is of the form: <name> IS <expression> and hence IS will be
     the fifth element of the tokenstream: 99 2 "<name> 1 "IS ... 
     or that the token is a comment. */

  if ((tokentally >= 5 && equalsymbol(fetch_array(tokens, 4), r_IS))
      || tokenprop == commentprop) {

    res = DEFN_SEQ(tree, false); /* attempt to parse a definition sequence */

    if (res == SUCCEED) {
      if (nexttoken == r_EOL) /* done if EOL encountered */
        return (SUCCEED);
     
      if (equalsymbol(nexttoken, r_SEMICOLON)) { /* check for trailing ";" */
        nialptr     t1;
        accept1();  /* accept the ";" */

        if (nexttoken == r_EOL) /* done if EOL encountered */
          return (SUCCEED);

        /* the global definition is in tree. Now look for a following
           expression sequence. If one is found, it will be tagged onto the
           definition sequence since eval evaluates either in the same way.
           This introduces a subtle constraint on eval. It should be done as
           a special parse tree node as suggested in blders.c . */

        res = EXPR_SEQ(&t1); /* attempt to parse an expression sequence */

        switch (res) {
          case SUCCEED:
            *tree = combinedaction(*tree, t1);  /* build the combined action */
            return (SUCCEED);
          case ERROR:
            freeup(*tree);
            return (ERROR);
          case FAIL:
            break;
        }

      }
      return (SUCCEED);      /* to indicate DEFN_SEQ. Caller must deal with
                                left over tokens */
    }
  }
  else
    res = EXPR_SEQ(tree);

  /* res is from one of the above calls to parser routines */

  if (res == ERROR && lasterror == undefined)
    error(undefined1, lasterrcnt);  /* report the deferred undefined identifier error */
  return (res);
}

/*   ROUTINE NAME : DEFN

     |----|
     definition-|->identifier--->| IS |-|-->ex_seq-------->
     |                |----| |
     |                       |-->op_seq-------->
     |                       |
     |                       |-->tr_form------->
     |
     | -->comment---->

     */

/* a comment is treated as a definition to permit spaces and
   description to be included at the top part of a nested
   definition.
*/


static nialint
DEFN(nialptr * tree, int inblock)
{
  nialptr     idlist,
              value,
              defaultvalue,
              sym,
              entr,
              var,
              newvar;
  nialint     si;            /* used to record stack_index */
  int         oldrole,
              role,
              foundrole = false;
  int         cnt,
              tries,
              res,
              k,
              externaldec;

  if (tokenprop == commentprop) /* treat a comment as a definition */
    return (COMMENT(tree));
  si = token_index;
  /* decide if dynamic or static definition. a dynamic one goes in global_symtab */

  /* get idlist for consistency with assignment */
  IDLIST(&idlist, (inblock ? statics : dynamic)); /* parse the identifier */

  if (tally(idlist) != 2) {  /* must be exactly one identifier */
    freeup(idlist);
    resettkn(si);
    return (FAIL);  /* to go on to expression case */
  }
  if (!equalsymbol(nexttoken, r_IS)) {
    freeup(idlist);
    resettkn(si);
    return (FAIL); /* to go on to expression case */
  }
  accept1();                 /* accepts IS */

  /* handle identifier */
  var = fetch_array(idlist, 1); /* select identifier */
  entr = get_entry(var);
  sym = get_sym(var);
  if (tag(var) == t_identifier) {
    oldrole = Rident;
    newvar = varinstall(get_id(var), oldrole, sym); /* put var in symbol tble sym */
    entr = get_entry(newvar);
    if (symprop(sym) != stpglobal) {
      st_s_valu(entr, createint(localcnt)); /* offset in local area */
      localcnt++;
    }
    replace_array(idlist, 1, newvar); /* update the idlist with the installed var */
    var = newvar;
  }
  else if (sym_flag(entr) && !instartup) { /* identifier is already defined */
    error(is_defined, 5);    /* 5 */
    freeup(idlist);
    return (ERROR);
  }
  else {
    oldrole = get_role(var);
    if (oldrole == Roptn || oldrole == Rexpr) { 
      nialptr     dfn_body = sym_valu(entr); /* get the body of current defn */

      if (tag(dfn_body) == t_expression || tag(dfn_body) == t_opform ||
          tag(dfn_body) == t_trform) {  /* has a scope */
        nialptr     dfn_sym = get_trsym(sym_valu(entr));  /* the symtab for the defn */

        d_clear_watches(dfn_sym, entr); /* clear all watches on the local
                                           variables of the dfn */
      }
    }
  }

  /* check for EXTERNAL definition */
  externaldec = equalsymbol(nexttoken, r_EXTERNAL);
  if (externaldec) {         
    accept1();  /* accept token */
    if (inblock) {           /* cannot declare externals within a block */
      error(inv_external, 9);/* 9 */
      freeup(idlist);
      return (ERROR);
    }

    /* determine role indicated by the keyword */
    if (equalsymbol(nexttoken, r_EXPRESSION)) {
      foundrole = Rexpr;
      defaultvalue = no_excode;
    }
    else 
    if (equalsymbol(nexttoken, r_OP) || equalsymbol(nexttoken, r_OPERATION)) {
      foundrole = Roptn;
      defaultvalue = no_opcode;
    }
    else 
    if (equalsymbol(nexttoken, r_TR) || equalsymbol(nexttoken, r_TRANSFORMER)) {
      foundrole = Rtrans;
      defaultvalue = no_trcode;
    }
    else 
    if (equalsymbol(nexttoken, r_VARIABLE)) {
      foundrole = Rvar;
      defaultvalue = no_value;
    }
    else {                   /* no valid keyword found */
      error(exp_external, 10);  /* 10 */
      freeup(idlist);
      return (ERROR);
    }

    accept1();               /* accept the keyword for the role */
    if (oldrole == Rident) { /* accept the new role */
      st_s_role(entr, foundrole);
      st_s_valu(entr, defaultvalue);
    }
    else 
    if (foundrole != oldrole)  
    { /* the declared role conflicts with the existing one */
      freeup(idlist);
      error(mismatch_ext, 11);  /* 11 */
      return (ERROR);
    }

    /* finish external declaration */
    *tree = b_ext_declaration(idlist, defaultvalue);
  }

  else 
  {    /* a definition to parse */
    if (equalsymbol(nexttoken, r_OP) || equalsymbol(nexttoken, r_OPERATION))
    { /* an opform to parse */
      if (oldrole == Rident)
        st_s_role(entr, Roptn);
      else if (oldrole != Roptn) {  /* name already matched to another role */
        freeup(idlist);
        error(role_error, 6);
        return (ERROR);
      }

      res = OP_FORM(&value); /* attempt to parse the operation form */

      switch (res) {
        case SUCCEED:
            break;           /* fall through to builder */
        case FAIL:           /* OP_FORM cannot FAIL */
        case ERROR:
            freeup(idlist);
            if (oldrole == Rident)
              st_s_role(entr, oldrole);
            return (ERROR);
      }
    }

    else 
    if (equalsymbol(nexttoken, r_LCURLY))
    {  /* an expression to parse */
      if (oldrole == Rident)
        st_s_role(entr, Rexpr); /* set role as an expression */
      else if (oldrole != Rexpr) {  /* name already matched to another role */
        freeup(idlist);
        error(role_error, 601);
        return (ERROR);
      }

      res = BLOCK(&value);  /* attempt to parse a BLOCK */

      switch (res) {
        case SUCCEED:
            break;           /* fall through to builder */
        case FAIL:           /* BLOCK cannot FAIL */
        case ERROR:
            freeup(idlist);
            if (oldrole == Rident)
              st_s_role(entr, oldrole);
            return (ERROR);
      }
    }

    else 
    if (equalsymbol(nexttoken, r_TR) || equalsymbol(nexttoken, r_TRANSFORMER))
    { /* a trform to parse */
      if (oldrole == Rident)
        st_s_role(entr, Rtrans);  /* set role as a transformer */
      else if (oldrole != Rtrans) { /* name already matched to another role */
        freeup(idlist);
        error(role_error, 18);
        return (ERROR);
      }

      res = TR_FORM(&value); /* attempt to parse a TR_FORM */

      switch (res) {
        case SUCCEED:
            break;           /* fall through to builder */
        case FAIL:           /* TR_FORM cannot FAIL */
        case ERROR:
            freeup(idlist);
            if (oldrole == Rident)
              st_s_role(entr, oldrole);
            return (ERROR);
      }
    }

    else {      
             
      /* If role is Rident the definition could be for any of the 3 object types. 
         We have to try all 3, any of which can recur. To parse recursive 
         definitions correctly, the role must be set during the parse. */

      tries = (oldrole == Rident ? 3 : 1);
      si = token_index;      /* backup point */
      res = FAIL;
      cnt = 0;
      while (cnt < tries && res != SUCCEED) {
        if (tries == 1)
          role = oldrole;
        else
          role = (cnt == 0 ? Rexpr : (cnt == 1 ? Roptn : Rtrans));
        lasterror = 0;
        st_s_role(entr, role);  /* install role for this try */

        res = formfinder(&value, &k); /* parse the object */

        switch (res) {
          case SUCCEED:
              switch (k) { 
                case s_P:
                    foundrole = Rexpr;
                    break;
                case s_O:
                    foundrole = Roptn;
                    break;
                case s_T:
                    foundrole = Rtrans;
                    break;
                case s_N:    /* nothing to parse */
                    error(exp_def, 7);  /* 7 */
                    freeup(idlist);
                    freeup(value);  /* freeup the Nulltree */
                    st_s_role(entr, oldrole);
                    return (ERROR);
#ifdef DEBUG
                default:
                    nprintf(OF_DEBUG, "parser shouldn't get here\n");
                    nabort(NC_ABORT);
#endif
              }
              if (tries == 1 && oldrole != foundrole) { 
                /* expected role not found */
                freeup(value);
                freeup(idlist);
                error(role_error, 29);
                return (ERROR);
              }
              else 
              if (role != foundrole) { /* rhs parsed, but role is not that assumed */
                freeup(value);
                res = FAIL;  /* fail so we can try again */
              }
              break;
          case FAIL:         /* this cannot happen, but succeed can produce one */
              break;
          case ERROR:
              if (cnt + 1 == tries) { /* report error on last try */
                freeup(idlist);
                st_s_role(entr, oldrole);
                if (lasterror == undefined) { /* deferred undefined error */
                  resettkn(deferredsi);
                  error(undefined1, lasterrcnt); /* post the undefined error */
                }
                return (ERROR);
              }
        }

        if (res != SUCCEED) {
          /* prepare to go around the loop again */
          resettkn(si);
          cnt++;
          if (res == ERROR && lasterror != undefined)
            freeup(apop());  /* to remove an error from a previous try */
        }
        else
          break;
      }

      /* have now done all tries */
      if (res == FAIL) {
        st_s_role(entr, Rident);
        error(exp_defn, 8);  /* 8 */
        freeup(idlist);
        return (ERROR);
      }
    }

    /* we reach here if we have parsed the definition body into parse tree value */

    /* We add the name of the definition to its local symbol table so
       it can be used in processing scoped variables. */

    if ((tag(value) == t_expression) ||
        (tag(value) == t_opform) || (tag(value) == t_trform)) {
      nialptr sym = (tag(value) != t_expression ? get_trsym(value) : get_exprsym(value));
      nialptr name = get_name(fetch_array(idlist, 1));

      if (sym != global_symtab)
        set_symtabname(sym, name);
    }


    /* breaklist handling. If the definition is an opform or an expression
       replacing one of the same kind, then the break flag will have been
       preserved and the entry should remain in the breaklist. If the
       definition is not of these types, the break flag will have been
       cleared because of the way "role" is installed. However, if the defn
       was previously one with a break flag set, the break list will
       erronously still include it. Here we avoid that problem */

    if (sym == global_symtab && (tag(value) != t_expression) && (tag(value) != t_opform))
      d_remove_break(sym_name(entr));

    /* finish definition parse tree */
    *tree = b_definition(idlist, value, get_role(var) != Rexpr);
  }


/* W do not store the definition's parse tree in the value cell until eval.
   Otherwise space gets eaten up on partial success of
   parsing. For global definitions we store the default
   object for the role, so eval has something to replace.
   For external declarations, we don't want to do this since it may overwrite
   existing valid definitions.
*/

  if (symprop(sym) == stpglobal && !externaldec)
    switch (get_role(var)) {
      case Rexpr:
          st_s_valu(entr, no_excode);
          break;
      case Rvar:
          st_s_valu(entr, no_value);
          break;
      case Roptn:
          st_s_valu(entr, no_opcode);
          break;
      case Rtrans:
          st_s_valu(entr, no_trcode);
          break;
    }

  return (SUCCEED);
}




/************************* EXPRESSION PARSING ************************/

/* Expression Parsing is done bottom-up, within the framework of the
   recursive descent parsing of the other language features.

   The expression parser works with a small set of tokens, some of which are
   recognized by the scanner, some of which are found by parts of the
   recursive descent parser.  These tokens and their recognizers are:
           literals           -  scanner
           identifiers        -  scanner
           parenthesis        -  scanner
           dot                -  scanner
           operator forms     -  parser (OP_FORM)
           transformer forms  -  parser (TR_FORM)

   The above tokens are obtained by the routine SHIFT and placed on the
   internal parse stack.  The stack is used as the datastructure for
   the application of reductions
     (e.g.  ...operator,operator -> ...atlas)

   The legitimate reductions are coded into the REDUCTION TABLE.  The
   rows correspond to the item one down in the stack, the columns
   correspond to the item at the top of the stack.  REDUCTION TABLE
   contains the addresses of the reduction routine which is to be
   executed when the specified condition exists at the top of the stack.

   The elements on the stack are actually doubles.  
   The first value gives the class of the item:
        primary,
        strand,
        array,
        operator,
        atlas,
        operation sequence,
        transformer,
        dot,
        left-end marker, or
        right-end marker.
   The second value is a  pointer to the semantic tree of the object 
   (or the null expr).
     
   These doubles are pushed and popped by the routines PUSHEXPR(type,tree)
   and tree=POPEXPR().  Typically, when a reduction is applied, the semantic
   trees of the top two items are combined by the reduction routine and
   the combined tree is pushed back on the stack.

   When no valid reduction is possible, there are two possible conditions:
     1) an error condition has occured - the parse stack is in an impossible
     condition, or 
     2) SHIFT is used to push another bottom-up token.

   The parse can recognize array expression, operations, or transformers.
   FORMFINDER is the general entry point to the bottom-up parser.  It attempts
   to find a reduction to array, operation, or transformer, and returns the
   type of the object it finds.  More specific routines (EXPRESSION, and
   OPERATION) invoke FORMFINDER with the expectation of finding one of the 
   three forms, and report failure if that form is not found.

   The use of return codes is somewhat nonstandard.  The codes SUCCEED, FAIL
   and ERROR are used by all routines.  DONE may be returned by APPLYFUN and
   the reduction routines (APPLYFUN does nothing but invoke the reduction
   routines - it is necessitated by syntatic restriction in C).  Anyway, the
   following details the use of return codes by the various groups of routines
   in the bottom-up parser:
       reduction routines      
          SUCCEED - reduction applied, FORMFINDER should continue to try reductions
          DONE - a final state was recognized. we have a legit array, op or tr.
          ERROR - a bad stack state has been detected (a state which cannot 
             possible reduce properly).

       SHIFT                   
          SUCCEED - a legit token has been found & stacked
          ERROR - the next token has logical inconsistancies.  It looks like some
             legit token, but has some error. (e.g. we find an OP followed by an
              invalid op_form.
            Note that when SHIFT runs into a token which is not part of an 
            expression, it stacks the right_end token and returns succeed.

       APPLYFUN - returns whatever the reduction routine or shift returned

       FORMFINDER              
          SUCCEED - a legit array, op or tr form was found (and is at top 
             of stack).
       ERROR - no valid form was found

       EXPRESSION,
       OPERATOR,
          SUCCEED - desired form was found.  tree is not on stack, but is 
             returned in routines *tree parameter.
          FAIL - a legit form was found, but not the desired one.
          ERROR - an illegal form was found.
*/


static int  statestack[STATELIMIT],  /* a parallel stack of the states */
            cntstack[STATELIMIT];   /* a parallel stack for counts of strand items
                                       or composition items */

static int  topexprstack,  /* index to the top of expression stack */
            topstate;   /* index to the top of the state stack */

static nialint eslimit;  /* global to keep track of expression stack size */

static nialptr exprstack;

/* routine to initialize the expression stack */

static void
startexprstack()
{
  eslimit = EXPRSTACKSIZE;
  exprstack = new_create_array(atype, 1, 0, &eslimit);
  topexprstack = -1;
  topstate = -1;
}

/* routine to clear the expression stack */

static void
cleanupexprstack()
{
  freeup(exprstack);
}


/* routine to extend the expression stack */

static void
extendexprstack()
{
  nialptr     newexprstack;

  eslimit += EXPRSTACKSIZE;
  newexprstack = new_create_array(atype, 1, 0, &eslimit);
  copy(newexprstack, 0, exprstack, 0, topexprstack);
  freeup(exprstack);
  exprstack = newexprstack;
}

/* additem combines the top two items on the stack by upping the
   count on the lower state and eliminating the top one.
  This is used to collect strands and compositions. The count of the entry
  is kept on the count stack and the state on the state stack. */

static void
additem(int state)
{
  topstate--;
  statestack[topstate] = state;
  cntstack[topstate] += 1;
}

/* create parse tree entry for the collection on top of the
   expr stack. This becomes the entry on top of the stack.
*/

static void
finishcollection()
{
  nialptr     res;
  int         i,
              state,
              cnt,
              newtag = 0,
              newstate = 0;

  state = statestack[topstate];
  cnt = cntstack[topstate];
  {
    nialint     c1 = cnt + 1;/* extra space for tag */

    res = new_create_array(atype, 1, 0, &c1); /* create the container */
  }

  /* store the appropriate tag */
  switch (state) {
    case s_A:
        newtag = t_strand;
        newstate = s_A;
        break;
    case s_OC:
        newtag = t_composition;
        newstate = s_O;
        break;
    case s_S:
        newtag = t_strand;
        newstate = s_A;
        break;
#ifdef DEBUG
    default:
        nprintf(OF_DEBUG, "unexpected state in finishcollection %d\n", state);
        nabort(NC_ABORT);
#endif
  }
  store_array(res, 0, createint(newtag));

  /* loop to store the stack entries in the container */
  for (i = 0; i < cnt; i++) {
    nialptr     itm;

    itm = fetch_array(exprstack, topexprstack);
    store_array(res, cnt - i, itm);
    decrrefcnt(itm); /* remove refcnt added in pushexpr */
    /* remove itm from exprstack */
    *(pfirstitem(exprstack) + topexprstack) = invalidptr;
    topexprstack--;
  }

  /* push the container to the expression stack and adjust state and count */
  ++topexprstack;
  store_array(exprstack, topexprstack, res);
  statestack[topstate] = newstate;
  cntstack[topstate] = 1;
}


#define state0 statestack[topstate]
#define state1 statestack[topstate-1]
#define state2 statestack[topstate-2]

#define  DONE  2             /* extra return value from reduce */

static nialptr bk_tree;
static int  bk_type;
static nialint backtoken,
            dots;

/*    ROUTINE NAME:  BACKUP
      back up a token, save it so shift can get it back on next call
*/

static void
backup()
{
  backtoken = true;
  bk_type = state0;
  bk_tree = popexpr();       /* no freeup because it is used later */
}

/*  service routine to put an expression state and 
    tree (possibly null) on the stack
*/

static void
pushexpr(int st, nialptr tree)
{ /* store the state checking that the state stack is not full */
  topstate++;
  if (topstate == STATELIMIT) {
    topstate = -1;
    exit_cover1("internal state stack overflow", NC_WARNING);
  }
  statestack[topstate] = st;

  /* set count for expression to 1 */
  cntstack[topstate] = 1;

  /* store the expression, extending the stack if necessary */
  topexprstack++;
  if (topexprstack == eslimit)
    extendexprstack();
  store_array(exprstack, topexprstack, tree);
}

/*  service routine to pop an expression state and tree from 
    the stack.  The tree value is returned.
*/

static      nialptr
popexpr()
{
  nialptr     itm;

#ifdef DEBUG
  if (topstate < 0) {
    nprintf(OF_DEBUG, "internal error in state stack\n");
    nabort(NC_ABORT);
  }
#endif
  /* build strands or compositions at top of the stack */
  if (cntstack[topstate] > 1)
    finishcollection();
  topstate--;

#ifdef DEBUG
  if (topexprstack < 0) {
    nprintf(OF_DEBUG, "internal error in expr stack\n");
    nabort(NC_ABORT);
  }
#endif

  itm = fetch_array(exprstack, topexprstack);
  /* remove itm from exprstack and remove refcnt added in pushexpr */
  decrrefcnt(itm);
  *(pfirstitem(exprstack) + topexprstack) = invalidptr;
  topexprstack--;
  return (itm);
}

/* routine to build a parened obj from the top expression and replace it */

static void
parentop()
{
  nialptr     itm;

  itm = fetch_array(exprstack, topexprstack);
  /* remove itm from exprstack and remove refcnt added in pushexpr */
  decrrefcnt(itm);
  /* put it back within a parened object */
  store_array(exprstack, topexprstack, b_parendobj(itm));
}


/*  
    CHOPSTACK cuts the stack back to and including the most
    recent s_LE.  All semantic trees encountered are freed.
    CHOPSTACK is used when an error is found by reduce or
    formfinder.
*/

static void
chopstack()
{
  while (dots >= 0) {
    while (state0 != s_LE)
      freeup(popexpr());
    freeup(popexpr());       /* LE */
    dots--;
  }
}

/*
      REDUCTION ROUTINES
      The following routines (and SHIFT) are applied by applyfun to the top
      of the expression parse stack according to REDUCTION TABLE.  These
      routines perform transformations on the top of the stack or report
      errors.  These routines can:
        SUCCEED - meaning formfinder should continue to applyfun
        DONE - meaning that a final state was recognized and formfinder
               has a complete form
        ERROR - meaning that an error was detected (bad stack state)
      These routines cannot FAIL.
*/

static nialint
Xpp()
{                            /* P P -> S */
  additem(s_S);
  return (SUCCEED);
}

static nialint
Xp_()
{                            /* P _ -> A */
  backup();
  statestack[topstate] = s_A;
  return (SUCCEED);
}

static nialint
Xsp()
{
  return (Xpp());
}                            /* S P -> S */

static nialint
Xs_()
{
  return (Xp_());           /* S -> P */
}

static nialint
Xao()
{
  return (XcmbR(s_O, b_curried));
}                            /* A O -> O */


static nialint
XaRE()
{
  return (XxRE(s_P));
}                            /* LE A RE -> DONE with P */

static nialint
Xoa()
{
  return (Xcmb(s_A, b_opcall));
}                            /* O A -> A */

static nialint
Xoo()
{                            /* O O -> C */
  additem(s_OC);
  return (SUCCEED);
}

static nialint
XoRE()
{
  return (XxRE(s_O));
}                            /* LE O RE -> DONE with O */

static nialint
XocRE()
{
  return (XxRE(s_O));
}                            /* LE OC RE -> DONE with O */

static nialint
Xocp()
{                            /* OC P -> O P */
  nialptr     t = popexpr();

  finishcollection();
  pushexpr(s_P, t);
  return (SUCCEED);
}

static nialint
Xoco()
{                            /* OC O -> OC */
  return (Xoo());
}

static nialint
Xoct()
{                            /* OC T -> O T */
  nialptr     t = popexpr();

  finishcollection();
  pushexpr(s_T, t);
  return (SUCCEED);
}

static nialint
Xto()
{
  return (Xtco());
}                            /* T O -> T */

static nialint
Xtco()
{
  return (Xcmb(s_O, b_transform));
}                            /* TC O -> T */

static nialint
XtcRE()
{
  return (XxRE(s_T));
}                            /* LE T RE -> T */

static nialint
Xt_()
{
  backup();
  statestack[topstate] = s_TC;
  return (SUCCEED);
}

static nialint
Xcmb(int state, nialptr(*b) (nialptr t1, nialptr t2))
{
  nialptr     t1,
              t2;

  t2 = popexpr();
  t1 = popexpr();
  pushexpr(state, (*b) (t1, t2));
  return (SUCCEED);
}

static nialint
XcmbR(int state, nialptr(*b) (nialptr t1, nialptr t2))
{
  nialptr     t1,
              t2;

  t1 = popexpr();
  t2 = popexpr();
  pushexpr(state, (*b) (t1, t2));
  return (SUCCEED);
}

static nialint
XxRE(int state)
{
  nialptr     t1;

  if (state2 == s_LE) {
    freeup(popexpr());       /* RE */
    t1 = popexpr();
    freeup(popexpr());       /* LE */
    pushexpr(state, t1);
    return (DONE);
  }
  else {
    error(exp_aot, 12);      /* 12 */
    return (ERROR);
  }
}

static nialint
XLERE()
{                            /* LE RE -> N */
  freeup(popexpr());         /* RE */
  freeup(popexpr());         /* LE */
  pushexpr(s_N, Nulltree);
  return (DONE);
}

static nialint
Xn_()
{                            /* N _ -> N */
  freeup(popexpr());
  return (SUCCEED);
}

static nialint
X_n()
{                            /* _ N -> N */
  freeup(popexpr());         /* N */
  freeup(popexpr());         /* LE */
  pushexpr(s_N, Nulltree);
  return (SUCCEED);
}

static nialint
XnRE()
{
  return (XxRE(s_N));
}                            /* N RE -> N */

static nialint
cant()
{                            /* illegal null form () (.) etc  */
  error(exp_aot, 13);        /* 13 */
  return (ERROR);
}



/*
      REDUCTION TABLE
      This table contains routines which are to be applied given
      the state of the top of the bottom-up parse stack.  The top
      token on the stack determines the column, the top-1 token
      determines the row.  REDUCE does the table lookup and invokes
      the selected routine.  The routines (below) act upon the top
      of the stack, report errors, or do nothing.
 
*/

typedef     nialint(*func_ptr) (void);


static func_ptr reducetab[100] = {
/*         ---p--- ---s--- ---a--- ---o--- --oc--- ---t--- --tc--- ---le-- ---re-- ---n---  */

/* P  */   Xpp,    cant,   cant,   Xp_,    cant,   Xp_,    Xp_,    shift,  Xp_,    X_n,
/* S  */   Xsp,    cant,   cant,   Xs_,    cant,   Xs_,    Xs_,    shift,  Xs_,    X_n,
/* A  */   cant,   cant,   cant,   Xao,    cant,   shift,  shift,  shift,  XaRE,   X_n,
   /* O  */ shift, shift, Xoa, Xoo, cant, shift, shift,
  shift, XoRE, X_n,
   /* OC */ Xocp, cant, cant, Xoco, cant, Xoct, cant,
  shift, XocRE, X_n,
   /* T  */ shift, shift, shift, Xto, cant, shift, shift,
  shift, Xt_, X_n,
   /* TC */ cant, cant, cant, cant, cant, cant, cant,
  cant, XtcRE, cant,
   /* LE */ shift, shift, shift, shift, shift, shift, shift,
  shift, XLERE, shift,
   /* RE */ cant, cant, cant, cant, cant, cant, cant,
  cant, cant, cant,
   /* N  */ Xn_, Xn_, Xn_, Xn_, Xn_, Xn_, Xn_,
  shift, XnRE, X_n
};


/* QUOTEPGM recognizes quoted-code primaries for SHIFT
   The valid forms are:
     !<ident>  and
     !( <expression> )   
*/


static nialint
quotepgm()

{
  nialptr     t1;
  int         res,
              k;

  accept1();           /* accept the "!" */
  if (tokenprop == identprop) {  /* a quoted identifier */
    nialptr     sym,
                entr;

    ID(&t1, passive);   /* parse the identifier, which will succeed */

    entr = get_entry(t1);
    if (entr == notfound) { /* no symbol table entry */
      error(undefined, 15);  /* 15 */
      freeup(t1);
      return (ERROR);
    }

    else 
    if (sym_role(entr) == Rident) { /* identifier not assigned */
      error(undefined, 16);  /* 16 */
      freeup(t1);
      return (ERROR);
    }
    else {  /* identifier found */

      res = IX_VAR(&t1); /* attempt to parse an indexed variable */

      if (res == SUCCEED) {
        pushexpr(s_P, b_parsetree(t1)); /* build the parse tree */
        return (SUCCEED);
      }

      if (res == ERROR) {
        freeup(t1);          /* since it contains the id */
        return (ERROR);
      }
 
      res = SCOPED_VAR(&t1);  /* attempt to parse a scoped variable */

      if (res == SUCCEED) {
        pushexpr(s_P, b_parsetree(t1)); /* build the parse tree */
        return (SUCCEED);
      }

      if (res == ERROR) {
        freeup(t1);          /* since it contains the id */
        return (ERROR);
      }

      if (sym_role(entr) == Rexpr) { /* the id is a named expression */
        sym = get_sym(t1);
        pushexpr(s_P, b_parsetree(b_expression((nialint) sym, (nialint) entr)));
        freeup(t1);
        return (SUCCEED);
      }

      if (current_env != Null) {
        nialptr     x = fetch_array(current_env, 0); /* get symbol table */

        if (symprop(x) == stpclosed) { /* a closed symbol table */
          append(referred, get_var_pv(t1));
          referred = apop(); /* update referred list */
        }
      }
      pushexpr(s_P, b_parsetree(t1));  /* build the parse tree for the variable */
      return (SUCCEED);
    }
  }

  /* look for an expression in parentheses */
  if (tokenprop == delimprop) {
    if (!equalsymbol(nexttoken, r_LPAREN)) {
      error(exp_quotepgm, 17);  /* 17 */
      return (ERROR);
    }

    else {
      accept1();    /* accept the "(" */

      res = ACTION(&t1);  /* attempt to parse an action */

      if (res == ERROR)
        return (ERROR);

      if (res == FAIL) {

        res = formfinder(&t1, &k);  /* attempt to parse a general expression */

        switch (res) {
          case SUCCEED:
              break;
          case FAIL:         /* cannot happen */
          case ERROR:
              return (ERROR);
        }
      }

      /* expression found, look for the closing parenthesis */
      if (equalsymbol(nexttoken, r_RPAREN)) {
        accept1();       /* accpet the ")" */
        pushexpr(s_P, b_parsetree(b_parendobj(t1))); /* build the parse tree */
        return (SUCCEED);
      }
      else {
        freeup(t1);
        error(exp_rparen, 19);  /* 19 */
        return (ERROR);
      }
    }
  }
  error(exp_quotepgm, 20);   /* 20 */
  return (ERROR);
}


/* rec_form handles things enclosed in parens for shift and PRIMARY.
   exprflag indicates that it is being called from PRIMARY and
   must return an expression. */

static nialint
rec_form(int exprflag)
{
  nialptr     t1;
  nialint     si;
  int         res,
              k;

  accept1();  /* accept the "(" */
  if (equalsymbol(nexttoken, r_RPAREN)) {
    error(exp_expr, 21);     /* 21 */
    return (ERROR);
  }

  si = token_index;     /* remember where this starts in case tokens are used up */

  res = EXPR_SEQ(&t1); /* attempt to parse an expression sequence */

  switch (res) {
    case SUCCEED:
        if (!equalsymbol(nexttoken, r_RPAREN)) { /* expect ")"  */
          error(exp_rparen, 22);  /* 22 */
          freeup(t1);
          return (ERROR);
        }
        accept1();  /* accept the ")" */
        pushexpr(s_P, b_parendobj(t1));  /* build the parse tree */
        return (SUCCEED);
    case ERROR:
        return (ERROR);
    case FAIL:
        break;
  }
  resettkn(si);              /* restore token index */
  if (exprflag)
    return (FAIL);

  /* look for operation or transformer */

  res = formfinder(&t1, &k);  /* attempt to find a general expression */

  switch (res) {
    case SUCCEED:
        pushexpr(k, t1);     /* result is assumed to be on expr stack */
        if (equalsymbol(nexttoken, r_RPAREN)) {
          if (state0 != s_N) { /* top item is not a Null */
            accept1();   /* accept the ")" */
            parentop();  /* put top item in parenened parse tree */
            return (SUCCEED);
          }
          else {
            error(ill_null_form, 23); /* 23 */
            freeup(t1);
            return (ERROR);
          }
        }
        else {
          error(exp_aot, 24);/* 24 */
          freeup(t1);
          return (ERROR);
        }
    case FAIL:               /* cannot happen */
    case ERROR:
        return (ERROR);

    default:
        return (ERROR);      /* to keep lint quiet */
  }
}

/* the routine brackets recognizes general list forms for shift */

static nialint
brackets()
{
  nialptr     t1,
              listholder;
  int         res,
              k;
  int         listkind = -1;
  nialint     listlimit,
              listcnt;

  accept1();                 /* accept left bracket */
  if (equalsymbol(nexttoken, r_RBRACKET) ||
      equalsymbol(nexttoken, r_ORBRACKET)) {
    accept1();               /* we have an empty list */
    pushexpr(s_P, st_list());
    return (SUCCEED);
  }

  else {  /* build the list of items */
    int         newkind = -1,
                newtag = t_list;

    listcnt = 0;
    listlimit = LISTSIZE;
    listholder = new_create_array(atype, 1, 0, &listlimit);
    do {
      if (listcnt > 0)
        accept1();           /* accept comma */

      res = formfinder(&t1, &k);  /* attempt to find a general expression */

      switch (res) {
        case SUCCEED:
            if (listcnt == 0) { /* set up the container based on first entry */
              switch (k) {
                case s_N:
                    t1 = Nulltree;
                case s_P:
                case s_S:
                case s_A:
                    newtag = t_list;
                    listkind = s_P;
                    break;
                case s_O:
                    newtag = t_atlas;
                    listkind = s_O;
                    break;
                case s_T:
                    error(invalid_optn, 270);
                    freeup(listholder);
                    return (ERROR);
              }
              /* start the list */
              store_array(listholder, 0, createint(newtag));
              store_array(listholder, 1, t1);
              listcnt++;
            }

            else {           /* second or subsequent entry */
              switch (k) {
                case s_N:
                    t1 = Nulltree;
                case s_P:
                case s_S:
                case s_A:
                    newkind = s_P;
                    break;
                case s_O:
                    newkind = s_O;
                    break;
                case s_T:
                    error(invalid_optn, 271);
                    freeup(listholder);
                    return (ERROR);
              }

              if (newkind == listkind) { /* new item is consistent */
                if (listcnt + 1 == listlimit) { /* expand list holder if necessary */
                  nialptr     newlistholder;
                  listlimit += LISTSIZE;
                  newlistholder = new_create_array(atype, 1, 0, &listlimit);
                  copy(newlistholder, 0, listholder, 0, listcnt + 1);
                  freeup(listholder);
                  listholder = newlistholder;
                }
                store_array(listholder, listcnt + 1, t1); /* offset by tag */
                listcnt++;
              }
              else { /* kind has switched, indicate an error and clear the list holder */
                switch (listkind) {
                  case s_P:
                      error(exp_expr, 25);
                      break; /* 25 */
                  case s_O:
                      error(exp_optn, 26);
                      break; /* 26 */
                  case s_T:
                      error(exp_tr, 27);
                      break; /* 27 */
                }
                freeup(t1);
                freeup(listholder);
                return (ERROR);
              }
            }
            break;

        default:
            freeup(listholder);
            return (ERROR);
      }
    }
    while (equalsymbol(nexttoken, r_COMMA));  /* funny C repeat-while construct */
  }
  /* put the object on the stack */
  if (equalsymbol(nexttoken, r_RBRACKET) ||
      equalsymbol(nexttoken, r_ORBRACKET)) {
    nialptr     newlistholder;

    accept1();  /* accept the "]" */
    listcnt += 1;            /* to count tag */
    newlistholder = new_create_array(atype, 1, 0, &listcnt);
    copy(newlistholder, 0, listholder, 0, listcnt);
    freeup(listholder);
    pushexpr(listkind, newlistholder);
    return (SUCCEED);
  }

  else {
    error(exp_rbracket, 28); /* 28 */
    freeup(listholder);
    return (ERROR);
  }
}


/* accept a parse tree.  If it represents a variable in the system
   symtab, and that var is bound to a basic ex, op, or tr,
   return the value bound to that variable,
   otherwise return the tree itself.  This accomplishes
   static lookup of system constants & should reduce user
   code density.  This is independent of the var's role, which
   is handled in shift.  When the bound value is returned,
   the input tree is freed, as it is usually something
   returned by a builder with a zero refcount.   
*/

static      nialptr
deref_sys(nialptr tree)
{ 
  if (tag(tree) == t_variable && sym_flag(get_entry(tree)))
  { nialptr ret;
    ret = fetch_var(get_sym(tree), get_entry(tree));
    /* test if the var is the name of a primitive */
    if (tag(ret) == t_basic) {
      freeup(tree);
      return (ret);
    }
  }
  return (tree);
}

/* SHIFT obtains the next bottom-up token and places it on the
   top of the parsestack.  SHIFT uses recursive descent
   routines to identify some of its tokens.  SHIFT will ERROR if
   any of these recursive descent routines do not SUCCEED. If SHIFT
   finds a bottom-up token it returns SUCCEED, if it fail to
   find a token, but does not encounter an ERROR, it stacks
   the right mark s_RE, and returns SUCCEED.  When SHIFT finds
   a simple token, it stacks it and returns SUCCEED.  SHIFT never
   returns FAIL.
*/

static nialint
shift()
{
  nialptr     t1;
  int         res,
              x;

  if (backtoken) {  /* a backup token exists, put it back on the stack */
    backtoken = false;
    pushexpr(bk_type, bk_tree);
    return (SUCCEED);
  }
  
  if (nexttoken == r_EOL || equalsymbol(nexttoken, r_SEMICOLON)) {
    /* store a right end marker on stack */
    pushexpr(s_RE, Nulltree);
    return (SUCCEED);
  }

  if (tokenprop >= constprop) { /* token is a constant */
    /* push the constant, type is computed from tokenprop */
    pushexpr(s_P, b_constant(tokenprop - constprop, nexttoken));
    accept1();
    return (SUCCEED);
  }

  /* handle the different tokens */

  switch (tokenprop) {
    case identprop:
       
        ID(&t1, passive); /* parse the identifier, which will succeed */

        if (tag(t1) == t_identifier || get_entry(t1) == notfound
            || get_role(t1) == Rident) { /* id is undefined */
          error(undefined, 32);
          freeup(t1);
          return (ERROR);
        }

        res = IX_VAR(&t1);  /* attempt to parse an indexed variable */

        if (res == SUCCEED) { /* push the indexed variable */
          pushexpr(s_P, t1);
          return (SUCCEED);
        }

        if (res == ERROR) {
          freeup(t1);        /* since it contains the id */
          return (ERROR);
        }

        res = SCOPED_VAR(&t1); /* attempt to parse a scoped variable */

        if (res == SUCCEED) { /* push the scoped variable */
          pushexpr(s_P, t1);
          return (SUCCEED);
        }
        if (res == ERROR) {
          freeup(t1);        /* since it contains the id */
          return (ERROR);
        }

        /* process the id directly */

        if (current_env != Null) { /* get the symbol table */ 
          nialptr     xx = fetch_array(current_env, 0);

          if (symprop(xx) == stpclosed) { /* add var to referred list */
            append(referred, get_var_pv(t1));
            referred = apop();
          }
        }

        /* push appropriate state based on role of id */

        x = get_role(t1);
        switch (x) {
          case Rexpr:
              pushexpr(s_P, deref_sys(
              b_expression((nialint) get_sym(t1), (nialint) get_entry(t1))));
              freeup(t1);
              return (SUCCEED);

          case Rconst:
          case Rvar:
             /* do not call deref_sys on t1 below. It causes a bug
                with global variable declared in defs.ndf */
              pushexpr(s_P, t1);
              return (SUCCEED);

          case Roptn:
              pushexpr(s_O, deref_sys(t1));
              return (SUCCEED);

          case Rtrans:
              pushexpr(s_T, deref_sys(t1));
              return (SUCCEED);
        }

    /* token is a delimiter, use top down routines to handle */
    case delimprop:

#ifdef SYNTACTIC_DOT
        if (equalsymbol(nexttoken, r_DOT)) {
          int         k;

          accept1();  /* accept the "." */
          pushexpr(s_LE, Nulltree);  /* mark the left end and count the dot */
          dots++;

          res = STMT_EXPR(&t1); /* attempt to parse stmts after dot  */

          switch (res) {
            case SUCCEED: /* push the result as a dotted object */
                pushexpr(s_P, b_dottedobj(t1));
                return (SUCCEED);

            case ERROR:
                return (ERROR);

            case FAIL:
                /* dot not followed by an expression sequence. */
                   
                res = formfinder(&t1, &k); /* look for an operation or transformer */

                if (res == SUCCEED)
                { pushexpr(k, b_dottedobj(t1));
                  return (SUCCEED);
                }
                
                if (res == ERROR)
                  return (ERROR)
          }
        }
#endif
        /* look for BLOCK marker */
        if (equalsymbol(nexttoken, r_LCURLY) || equalsymbol(nexttoken, r_BEGIN)) {

          res = BLOCK(&t1);  /* attempt to parse the block */

          switch (res) {
            case SUCCEED:
                pushexpr(s_P, t1);  /* push the block */
                return (SUCCEED);

            default:
                return (ERROR);
          }
        }

        /* look for a left parenthesis starting a recursive form */
        if (equalsymbol(nexttoken, r_LPAREN))
          return (rec_form(false));

        /* look for an operation form marker */
        if (equalsymbol(nexttoken, r_OP) || equalsymbol(nexttoken, r_OPERATION)) {

          res = OP_FORM(&t1);  /* attempt to parse an OPFORM */

          switch (res) {
            case SUCCEED:
                pushexpr(s_O, t1);
                pushexpr(s_RE, Nulltree); /* nothing can follow */
                return (SUCCEED);

            default:
                return (ERROR);
          }
        }

        /* look for a transformer form marker */
        if (equalsymbol(nexttoken, r_TR) || equalsymbol(nexttoken, r_TRANSFORMER)) {

          res = TR_FORM(&t1); /* attempt to parse a TRFORM */

          switch (res) {
            case SUCCEED:
                pushexpr(s_T, t1);
                pushexpr(s_RE, Nulltree); /* nothing can follow */
                return (SUCCEED);

            default:
                return (ERROR);
          }
        }

        /* look for "!" as a PGM quote */
        if (equalsymbol(nexttoken, r_PGMQUOTE))
          return (quotepgm());

        /* look for a list forming bracket "[" */

        if (equalsymbol(nexttoken, r_LBRACKET) || equalsymbol(nexttoken, r_OLBRACKET)) {
          return (brackets());
        }
  }

  /* no tokens recognized */
  pushexpr(s_RE, Nulltree);
  return (SUCCEED);
}


/* FORMFINDER accepts a token pointer,  parses for a form of
   type array, operation, or transformer, and returns SUCCEED, ERROR,
   or FAIL along with the type of the form found 'k', its semantic
   tree 'tree', and the updated token pointer 'i'.  If ERROR or
   FAIL are returned, any semantic trees built are freed.
   FORMFINDER is recursive.
*/

static nialint
formfinder(nialptr * tree, int *k)
{
  int         res,
              save_dots,
              done;
  nialint     (*fun) (void);

#ifdef USER_BREAK_FLAG
  checksignal(NC_CS_NORMAL);
#endif
  save_dots = dots;
  dots = 0;
  backtoken = false;
  pushexpr(s_LE, Nulltree);  /* push a left end mark */

  res = shift();  /* attempt a shift */

  switch (res) {
    case ERROR:
        chopstack();
        dots = save_dots;
        return (ERROR);

    case FAIL:
#ifdef DEBUG
        nprintf(OF_DEBUG, "shift cannot fail in formfinder\n");
        nabort(NC_ABORT);
#else
        error(parser_error, 30);  /* 30 */
        return (ERROR);
#endif

    case SUCCEED:
        break;
  }

  /* shift has succeeded, loop doing reductions */
  done = false;
  while (!done) { /* get reduction routine from the table */
    fun = reducetab[state0 + (state1 * 10)];  /* depends on number of states (= 10) */

    res = (*fun) ();  /* do the reduction */

    switch (res) {
      case SUCCEED:
          break;             /* to continue in the loop */

      case FAIL:
#ifdef DEBUG
          nprintf(OF_DEBUG, "applied funs cannot fail in formfinder\n");
          nabort(NC_ABORT);
#else
          error(parser_error, 31);  /* 31 */
          return (ERROR);
#endif

      case ERROR:
          chopstack();
          dots = save_dots;
          return (ERROR);

      case DONE:  /* reductions have been completed */
          if (dots > 0) {
            dots--;
            break;
          }

          /* get the state and tree from the stacks */
          *k = state0;
          *tree = popexpr();

          dots = save_dots;
          done = true;
          break;       /* returns to loop which exits */  
    }
  }
  return (SUCCEED); /*  SUCCEED is returned after loop ends */
}


/* EXPRESSION invokes the bottom-up parsing logic to find an
   array form.  It can SUCCEED, FAIL, or ERROR.  It can
   recurse via EXPR_SEQ.
*/

static nialint
EXPRESSION(nialptr * tree)
{
  nialint     si;
  int         res,
              k;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "EXPRESSION \n");
#endif
  si = token_index; /* remember the token position */

  res = formfinder(tree, &k);  /* do a general expression parse */

  switch (res) {
    case SUCCEED:
        switch (k) {
          case s_P:
          case s_S:
          case s_A:
              return (SUCCEED);
          
          /* have parsed an operation or transformer */
          default:
              freeup(*tree);
              resettkn(si);  /* reset token_index to back out of an operation
                                or transformer that does not turn out to be part 
                                of an expression */
              return (FAIL);
        }

    case FAIL:
        return (FAIL);

    case ERROR:
        return (ERROR);
    default:
        return (ERROR);      /* to make lint shut up */
  }
}


/* PRIMARY is similar to SHIFT but returns a primary form
   directly rather than pusing it on the expression stack.  
   It can SUCCEED, FAIL, or ERROR.  
   It can recurse due EXPR_SEQ. 
   Only used by the IX_VAR routine.
*/

static nialint
PRIMARY(nialptr * tree)
{
  nialptr     t1;
  int         res,
              x;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "PRIMARY \n");
#endif

  /* FAIL if at end of expression */
  if (nexttoken == r_EOL || equalsymbol(nexttoken, r_SEMICOLON))
    return (FAIL);

  /* look for a constant and retrun corresponding tree */
  if (tokenprop >= constprop) {
    *tree = b_constant(tokenprop - constprop, nexttoken);
    accept1();
    return (SUCCEED);
  }

  /* handle other token cases */

  switch (tokenprop) {

    case identprop:
     
        ID(&t1, passive); /* parse the identifier, which will succeed */

        if (tag(t1) == t_identifier || get_entry(t1) == notfound
            || get_role(t1) == Rident) { /* id is undefined */
          error(undefined, 44); /* 44 */
          freeup(t1);
          return (ERROR);
        }

        res = IX_VAR(&t1);  /* attempt to parse an indexed variable */

        if (res == SUCCEED) { /* push the indexed variable */
          *tree = t1;
          return (SUCCEED);
        }

        if (res == ERROR) {
          freeup(t1);        /* since it contains the id */
          return (ERROR);
        }

        res = SCOPED_VAR(&t1); /* attempt to parse a scoped variable */

        if (res == SUCCEED) { /* push the scoped variable */
          *tree = t1;
          return (SUCCEED);
        }
        if (res == ERROR) {
          freeup(t1);        /* since it contains the id */
          return (ERROR);
        }

        /* process the id directly */

        if (current_env != Null) { /* get the symbol table */ 
          nialptr     xx = fetch_array(current_env, 0);

          if (symprop(xx) == stpclosed) { /* add var to referred list */
            append(referred, get_var_pv(t1));
            referred = apop();
          }
        }

        /* return appropriate parse tree based on role */

        x = get_role(t1);
        switch (x) {
          case Rexpr:
              *tree = b_expression((nialint) get_sym(t1), (nialint) get_entry(t1));
              freeup(t1);
              return (SUCCEED);

          case Rconst:
          case Rvar:
              *tree = t1;
              return (SUCCEED);

          case Roptn:
          case Rtrans:
              freeup(t1);
              error(exp_primary, 33); /* 33 */
              return (ERROR);
        }

    /* token is a delimiter, use top down routines to handle */

    case delimprop:
        /* look for BLOCK marker */
        if (equalsymbol(nexttoken, r_LCURLY) || equalsymbol(nexttoken, r_BEGIN)) {

          res = BLOCK(&t1);  /* attempt to parse the block */

          switch (res) {
            case SUCCEED:
                *tree = t1;  /* push the block */
                return (SUCCEED);

            default:
                return (ERROR);
          }
        }

        /* look for a left parenthesis starting a recursive form */
        if (equalsymbol(nexttoken, r_LPAREN))
          res = rec_form(true);

        else
        /* look for a program quote "!" */
        if (equalsymbol(nexttoken, r_PGMQUOTE))
          res = quotepgm();

        else 
        /* look for list bracket opener  */
        if (equalsymbol(nexttoken, r_LBRACKET) || equalsymbol(nexttoken, r_OLBRACKET))
          res = brackets();

        else
          res = FAIL;

        if (res == SUCCEED)
          *tree = popexpr();  /* pick up result from the expr stack */
        return (res);
  } 
  return (FAIL);
}


/* OPERATION uses the general expression parse to look for an operation.
*/

static nialint
OPERATION(nialptr * tree)
{
  int         k,
              res;

  res = formfinder(tree, &k); /* do general expression parse */

  switch (res) {
    case SUCCEED:
        switch (k) {
          case s_O:
              return (SUCCEED);
          default:
              freeup(*tree);
              return (FAIL);
        }
    case FAIL:
        return (FAIL);
    case ERROR:
        return (ERROR);
    default:
        return (ERROR);      /* to make lint shut up */
  }
}


/*
      ROUTINE NAME : OP_FORM

          |----|             |---|                 |---|
op_form-->| OP |-->idlist-|->| ( |----->expr_seq-->| ) |-->
          |----|          |  |---|                 |---|
                          |
                          |------------>block------------->

*/

static nialint
OP_FORM(nialptr * tree)
{
  nialptr     args,
              body,
              entr,
              sym,
              id,
              s_env;
  nialptr     var,
              newvar;
  int         j,
              res,
              nargs,
              s_no;

  /* look for the keyword OP or OPERATION */
  if ((equalsymbol(nexttoken, r_OP)) ||(equalsymbol(nexttoken, r_OPERATION))) {
    accept1();  /* accept the keyword */

    IDLIST(&args, formal); /* attempt to parse the IDLIST */

    if (tally(args) == 1) { /* an empty Id list found */
      error(exp_ident, 34);
      freeup(args);
      return (ERROR);
    }  
                      /* 34 */
    nargs = tally(args) - 1;

    /* set up local symbol table, assume it is a non-block */

    s_no = localcnt;  /* remember local cnt */
    localcnt = 0;

    s_env = current_env;
    incrrefcnt(current_env); /* protect current_env */
    sym = addsymtab(stpopen, "ANONYMOUS");
    hitch(sym, current_env); /* new symbol table added to front current env */
    current_env = apop();

    /* place the arguments in the local symbol table */
    for (j = 1; j <= nargs; j++) {
      var = fetchasarray(args, j);  /* select identifier */
      id = get_id(var);
      newvar = varinstall(id, Rvar, sym);
      entr = get_entry(newvar);
      st_s_valu(entr, createint(localcnt)); /* offset to the argument */
      localcnt++;
      replace_array(args, j, newvar);
    }

    /* look for a "(" or "." indicating the body is an expression */
#ifdef SYNTACTIC_DOT
    if (equalsymbol(nexttoken, r_LPAREN) || equalsymbol(nexttoken, r_DOT))
#else
    if (equalsymbol(nexttoken, r_LPAREN))
#endif
    { /* the parenthesis or DOT is handled by expression */

      res = EXPRESSION(&body); /* attempt to parse the expression */

      switch (res) {
        case SUCCEED:  /* build the opform */
            *tree = b_opform(sym, current_env, localcnt, args, body);
            goto restoreenv;

        case FAIL:
            res = ERROR;
            error(exp_body, 35);  /* 35 */
            /* fall through to ERROR case */

        case ERROR:
            freeup(args);
            goto restoreenv;
      }
    }
    else 

    /* look for a BLOCK marker */
    if (!equalsymbol(nexttoken, r_LCURLY) && !equalsymbol(nexttoken, r_BEGIN)) {
      error(exp_body, 37);   /* first token must be a paren or brace */
      freeup(args);
      res = ERROR;
      goto restoreenv;
    }

    accept1(); /* accept the block marker */
    replace_symprop(sym, stpclosed);  /* change the symbol table type */

    res = BLOCKBODY(sym, args, &body); /* attempt to parse the BLOCK body */

    if (res != SUCCEED) {
      freeup(args);
      goto restoreenv;       /* cannot have res == FAIL */
    }

    /* build the op form */
    *tree = b_opform(sym, current_env, localcnt, args, body);

    /* restore environment */
restoreenv:
    freeup(current_env);     /* frees up sym if failure or error has occurred */
    current_env = s_env;
    decrrefcnt(current_env); /* remove protection set earlier */
    localcnt = s_no;
    return (res);
  }
  return (FAIL);
}


/*
  ROUTINE NAME : TR_FORM

          |-------|
tr_form-->| TRANS |----->arg_list-----s_operation------>
          |-------|
*/

static nialint
TR_FORM(nialptr * tree)
{
  nialptr     sym,
              var,
              newvar,
              id,
              args,
              entr,
              body;
  int         res,
              nargs,
              j;

  /* look for keyword TR or TRANSFORMER */
  if (equalsymbol(nexttoken, r_TR) || equalsymbol(nexttoken, r_TRANSFORMER)) {
    nialptr     save_env = current_env;

    accept1();  /* accept the keyword */

    IDLIST(&args, formal); /* attempt to parse the id list */
    if (tally(args) == 1) {  /* empty id list found */
      error(exp_ident, 371);  /* 371 */
      freeup(args);
      return (ERROR);
    }  
                      
    nargs = tally(args) - 1;
    if (nargs > 0) {         /* set up parameter symbol table */
      incrrefcnt(current_env); /* protect the current env */
      sym = addsymtab(stpparameter, "ANONYMOUS");
      hitch(sym, current_env);  /* add the new symbol table to the front */
      current_env = apop();

      /* store the arguments in the local symbol table */
      for (j = 0; j < nargs; j++) {
        var = fetchasarray(args, (nialint) (j + 1));  /* select identifier */
        id = get_id(var);
        newvar = varinstall(id, Roptn, sym);
        entr = get_entry(newvar);
        st_s_valu(entr, createint((nialint) (j)));
        /* j  is the argument offset */
        replace_array(args, (nialint) (j + 1), newvar);
      }
    }
    else
      sym = Null;  /* no local symbols */

    res = OPERATION(&body);  /* attempt to parse the body as an operation */

    switch (res) {
      case SUCCEED:  /* build the transformer form */
          *tree = b_trform(sym, current_env, args, body);
          break;

      case FAIL:
          error(exp_optn, 38);  /* 38 */
          res = ERROR;       /* FAIL falls through into ERROR */

      case ERROR:
          freeup(args);
          freeup(sym);
          goto cleanup;
    }

    /* restore environment */
cleanup:if (nargs > 0) {
      freeup(current_env);
      current_env = save_env;
      decrrefcnt(current_env);  /* unprotect current_env set earlier */
    }
    return (res);
  }
  return (FAIL);
}


/*
   ROUTINE NAME :  EXPR_SEQ

   expr_seq ------->|------->stmt_expr----->|-->|--------->|--->
                    |                       |   |          |
                    |      |-----|          |   |   |---|  |
                    |<-----|  ;  |<---------|   |-->| ; |->|
                           |-----|                  |---|

    parses a sequence of expressions. An exprseq is built even if there
    is only one expression. (The extra layer for one expression is needed
    to support the Nial-level debugging features. The code could be simplified
    since it was designed to avoid this extra layer with only one found.)
*/

static nialint
EXPR_SEQ(nialptr * tree)
{
  nialptr     t1,
              t2;
  int         res;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "EXPR_SEQ \n");
#endif

  res = STMT_EXPR(&t1);  /* attempt to parse a STMT_EXPR */

  if (res == ERROR) {
    return (ERROR);
  }

  if (res == SUCCEED) {
    if (equalsymbol(nexttoken, r_SEMICOLON)) {
      accept1(); /* accept the ";" */

      res = STMT_EXPR(&t2);  /* attempt to parse a second STMT_EXPR */

      if (res == ERROR) {
        freeup(t1);  
        return (ERROR);
      }

      if (res == SUCCEED) {
        /* start the expression sequence */
        t1 = st_exprseq(t1);

        /* loop to add the next expression and attempt to parse another */
        while (res == SUCCEED) {
          /* add the next one */
          t1 = nial_extend(t1, t2);

          if (equalsymbol(nexttoken, r_SEMICOLON)) {
            accept1(); /* accept the ";" */

            res = STMT_EXPR(&t2); /* attempt to parse another STMT_EXPR */

            if (res == ERROR) {
              freeup(t1);
              return (ERROR);
            }

            if (res == FAIL)
              t1 = nial_extend(t1, Nulltree);  /* add Nulltree to indicate 
                                                  a trailing ";" */
          }
          else
            res = FAIL;  /* sequence has ended */

          checksignal(NC_CS_NORMAL);
        }                    /* end while     */

        /* return the result */
        *tree = t1;
        return (SUCCEED);
      } 

      /* a ";" found so add Nulltree */
      t1 = st_exprseq(t1);
      t1 = nial_extend(t1, Nulltree);
    }

    /* no ";" found return the STMT_EXPR as an t_exprseq node */

    if (tag(t1) != t_exprseq)
      t1 = st_exprseq(t1);   /* to assist debugging code */
    *tree = t1;
    return (SUCCEED);
  }
  return (FAIL);
}


/*
   ROUTINE NAME : DEFN_SEQ

   defn_seq ------->|------->definition---->|-------->
                    |                       |
                    |      |-----|          |
                    |<-----|  ;  |<---------|
                           |-----|

    parses a sequence of definitions. 
*/

static nialint
DEFN_SEQ(nialptr * tree, int inblock)
{
  nialptr     t1,
              t2;
  nialint     si;
  int         res;

  res = DEFN(&t1, inblock); /* attempt to parse a definition */

  if (res != SUCCEED)
    return (res);

  si = token_index;  /* remember the token position */

  /* look for a ";" */
  if (equalsymbol(nexttoken, r_SEMICOLON)) {
    accept1(); /* accept the ";" */

    t1 = st_defnseq(t1);   /* start the definition sequence */

    res = DEFN(&t2, inblock); /* attempt to parse a second definition */

    /* loop while finding definitions adding them to the sequence */
    while (res == SUCCEED) {
      t1 = nial_extend(t1, t2);
      si = token_index;  /* remember the  new token position */
      /* look for a ";" */
      if (equalsymbol(nexttoken, r_SEMICOLON)) {
        accept1(); /* accept the ";" */

        res = DEFN(&t2, inblock); /* attempt to parse another definition */

        if (res == ERROR) {
          freeup(t1);
          return (res);  /* return out of loop */
        }
      }
      else
        res = FAIL;  /* set to end loop */
    }                        /* end while   */

    if (res == ERROR) {
      freeup(t1);
      return (res);
    }

  }

  /* loop has ended with one or more definitions stored in t1 */
  resettkn(si);              /* so that last semicolon is not used up */
  *tree = t1;
  return (SUCCEED);
}

/*
  ROUTINE NAME :BLOCK

          |---|                                        
block---->| [ |----->|
          |---|      |                                  
                     |   |-------|           |---|               
                     |-->| LOCAL |-->idlist--| ; |---|
                     |   |-------|           |---|   |
                     |                               |
                     |<-----------------|<-----------|
                     | 
                     |   |----------|           |---|   
                     |-->| NONLOCAL |-->idlist--| ; |---|
                     |   |----------|           |---|   |
                     |                                  |
                     |<-----------------|<--------------|
                     | 
                     | 
                     |------defn_seq-->|
                     |                 |
                     |                 |                    |---|
                     |-----------------|---->expr_seq------>| ] |------->
                                                            |---|


A block is a local list (optional), followed by a nonlocal list (optional),
followed by a definition sequence (optional), followed by an expression sequence.

The routine is broken into two parts to meld the body part with the operation form
definition.

*/

static nialint
BLOCK(nialptr * tree)
{
  nialptr     sym,
              s_env,
              block_bdy;
  int         res,
              s_no;

  accept1(); /* accept the BLOCK marker */

  /* set up local symbol table, assume it is a block */
  s_env = current_env;
  incrrefcnt(current_env);
  sym = addsymtab(stpclosed,"ANONYMOUS");
  hitch(sym, current_env);
  current_env = apop();

  /* set up for locals */
  s_no = localcnt;
  localcnt = 0;

  res = BLOCKBODY(sym, Null, &block_bdy);  /* attempt to parse the block body */

  if (res == SUCCEED)
    *tree = b_block(sym, current_env, localcnt, block_bdy);

  /* restore environment */
  freeup(current_env);       /* which will free up sym if BLOCKBODY had an error */
  current_env = s_env;
  decrrefcnt(current_env);
  localcnt = s_no;
  return (res);
}

static nialint
BLOCKBODY(nialptr sym, nialptr args, nialptr * block)
{
  nialptr     var,
              newvar,
              id,
              s_nonlocs,
              s_referred;
  nialptr     locallist,
              nonlocallist,
              defs,
              body;
  nialint     nlocs,
              j,
              k;
  int         res;

  /* save previous scope information */
  s_nonlocs = nonlocs;
  s_referred = referred;

  /* create new scope information */
  referred = Null;

  /* The coding below assumes LOCAL declarations precede NONLOCAL ones and
     that no comments intercede between the block open and these
     declarations. To handle the latter would be messy since they would have
     to be stuffed into parse tree and regenerated in the right order in
     deparse. 
*/

  /* parse the local declaration list if any */
  if (equalsymbol(nexttoken, r_LOCAL)) {
    accept1(); /* accept the symbol LOCAL */

    IDLIST(&locallist, formal);  /* parse the id list */

    /* look for the ";" */
    if (!equalsymbol(nexttoken, r_SEMICOLON)) {
      error(exp_semicolon, 39); /* 39 */
      freeup(locallist);
      res = ERROR;
      goto restorescope1;
    }

    accept1();  /* accept the ";" */

    /* add local list into the local symtab */
    nlocs = tally(locallist) - 1;
    for (j = 1; j <= nlocs; j++) {
      id = get_id(fetch_array(locallist, j)); /* select identifier */
      newvar = varinstall(id, Rvar, sym);
      st_s_valu(get_entry(newvar), createint(localcnt));  /* offset */
      localcnt++;
      replace_array(locallist, j, newvar);
    }
  } 
  else /* no locals */
    locallist = Null;

  /* parse the nonlocal declaration list if any */
  if (equalsymbol(nexttoken, r_NONLOCAL)) {
    accept1(); /* accept the symbol NONLOCAL */

    IDLIST(&nonlocallist, nonlocal);  /* parse the id list */

   /* build the nonlocal list */
    nlocs = tally(nonlocallist) - 1;
    for (j = 1; j <= nlocs; j++) {
      var = fetch_array(nonlocallist, j); /* select identifier */
      id = get_name(var);    /* var could be ident or var */

      /* check for conflict with args */
      for (k = 1; k < tally(args); k++) { /* start at 1 to skip idlist tag */
        nialptr     argname = get_name(fetch_array(args, k));

        if (id == argname) {
          error(arg_reused, 40);  /* 40 */
          freeup(locallist);
          freeup(nonlocallist);
          res = ERROR;
          goto restorescope1;
          /* no freeup of nonlocs since not yet assigned */
        }
      }

      /* check for conflict with LOCAL list */
      for (k = 1; k < tally(locallist); k++) {  /* start at 1 to skip idlist tag */
        if (id == get_var_pv(fetch_array(locallist, k))) {
          error(local_reused, 401); /* 401 */
          freeup(locallist);
          freeup(nonlocallist);
          res = ERROR;
          goto restorescope1;
          /* no freeup of nonlocs since not yet assigned */
        }
      }

      if (tag(var) == t_identifier) {
        newvar = varinstall(id, Rvar, get_sym(var));
        replace_array(nonlocallist, j, newvar);
      }
    }

    /* look for the ";" */
    if (!equalsymbol(nexttoken, r_SEMICOLON)) {
      error(exp_semicolon, 402);  /* 402 */
      freeup(locallist);
      freeup(nonlocallist);
      res = ERROR;
      goto restorescope1;    /* avoid freeup of nonlocs since not yet
                              * assigned */
    }

    accept1(); /* accept the ";" */

  }
  else  /* no nonlocal list */
    nonlocallist = st_idlist();

  nonlocs = nonlocallist; /* store nonlocal list in a global  for use by symtab.c */

  res = DEFN_SEQ(&defs, true);  /* attempt to parse a definition sequence */

  if (res == ERROR) {
    freeup(locallist);
    goto restorescope;
  }

  if (res == FAIL)
    defs = grounded;
  else  /* definition sequence found */

   /* check for last semicolon at end of sequence */ 
  if (equalsymbol(nexttoken, r_SEMICOLON))
    accept1(); /* accept the ";" */
  else {
    error(exp_semicolon, 41);/* 41 */
    res = ERROR;
    freeup(locallist);
    freeup(defs);
    goto restorescope;
  }

  res = EXPR_SEQ(&body); /* attempt to parse the expression sequence */

  if (res == SUCCEED) {
    /* look for end of block marker */
    if (equalsymbol(nexttoken, r_RCURLY) || equalsymbol(nexttoken, r_END)) {
      accept1(); /* accept the end of block marker */
      
      replace_symprop(sym, stpopen);  /* after parsing the symbol table is opened 
                                         so that dynamically created names will 
                                         not go into the local table */

      *block = b_blockbody(locallist, nonlocallist, defs, body);
      goto restorescope;
    }
    else {
      error(exp_rcurly, 42); /* 42 */
      res = ERROR;
      freeup(locallist);
      freeup(defs);
      freeup(body);
    }
  }
  else {
    if (res == FAIL) { /* force an error since expression sequence is needed */
      error(exp_eseq, 43);
      res = ERROR;
    }                        /* 43 */
    freeup(locallist);
    freeup(defs);
  }

  /* do clean up and return res as set above */
restorescope: 
  freeup(nonlocs);
restorescope1:
  freeup(referred);
  nonlocs = s_nonlocs;
  referred = s_referred;
  return (res);
}


/* The parsing of statement_expressions is accelerated by
   attempting to parse an expression initially, and then checking
   whether it could also be the head of an assignment statement.
   To do this undefined errors have to be deferred. To avoid
   unnecessary work the error "undefined" is not processed
   but its location is remembered so that the error can be posted
   later.
   For error "undefined", nothing is put on the stack, thus
   at the end of the routine, error(undefined1) is called to
   create the actual error message.

   ROUTINE NAME :   STMT_EXPR

  stmt_expr--|------->assign_expr------>|------------------>
             |                          |
             |------->while_expr------->|
             |                          |
             |------->repeat_expr------>|
             |                          |
             |------->for_expr--------->|
             |                          |
             |------->if_expr---------->|
             |                          |
             |------->case_expr-------->|
             |                          |
             |------->comment---------->|
             |                          |
             |------->expr------------->|
             |                          |
             |------->break------------>|
             |                          |
             |------->return----------->|

*/


static nialint
STMT_EXPR(nialptr * tree)
{
  int         res,
              erroronfirsttry;
  nialint     si;
  nialptr     save_referred;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "STMT_EXPR \n");
#endif

  if (nexttoken == r_EOL)
    return (FAIL);

  si = token_index;  /* remember token position */
  /* first we try for an expression  */
  erroronfirsttry = false;
  incrrefcnt(referred);      /* save current set of referred identifiers */
  save_referred = referred;

  res = EXPRESSION(tree); /* attempt to parse an expression */

  /* optimization -- if EXPRESSION succeeds, then either we can use its
     result, or it could be part of an assignment. By putting this test first
     we avoid backing up long strands that fail in assignment. If an error of
     "undefined" occurs, we want to try the assignment code as it may
     succeed. 
*/
  switch (res) {
    case SUCCEED:
        if (equalsymbol(nexttoken, r_GETS) ||
            equalsymbol(nexttoken, r_ASSIGNSYM)) {
          freeup(*tree);
          freeup(referred);
          referred = save_referred; /* restore previous set of referred
                                     * identifiers */
          goto assigns;      /* must be assign or else it is an error */
        }
        decrrefcnt(save_referred);  /* clear out saved referreds */
        if (referred != save_referred)
          freeup(save_referred);
        return (SUCCEED);

    case ERROR:              /* if error is caused by undefined variable, as
                                stored in lasterror, attempt to parse
                                assignment forms */
        if (lasterror != undefined) { /* some other error */
          decrrefcnt(save_referred);  /* clear out saved referreds */
          if (referred != save_referred)
            freeup(save_referred);
          return (ERROR);
        }
        erroronfirsttry = true;
        goto assigns;        /* might be an assignment */

    case FAIL:
        /* fall through to try assigns, since it may fail in a block using a
           name that has a role in an outer block , i.e. {choose gets 5} */
        break;
  }

/* try for an assignment expression */
assigns:
  freeup(referred);
  referred = save_referred;  /* restore previous set of referred iden-
                              * tifiers */
  decrrefcnt(referred);
  resettkn(si);              /* reset in case EXPRESSION has used up tokens */

  res = ASSIGN_EXPR(tree); /* attempt to parse assignment expression */

  switch (res) {
    case SUCCEED:
        return (res);

    case ERROR:
        if (lasterror == undefined)
          error(undefined1, lasterrcnt);
        return (res);

    case FAIL:
        break;
  }

  res = IX_ASSIGN_EXPR(tree);  /* attempt to parse an indexed assignment expression */

  switch (res) {
    case SUCCEED:
        return (res);
    case ERROR:

        if (lasterror == undefined)
          error(undefined1, lasterrcnt);
        return (res);

    case FAIL:
        /* since EXPR had an error above (rather than just a fail), we know
           none of the key-word constructs follow, so we should report the
           latest deferred error. */
        if (erroronfirsttry) {
          resettkn(deferredsi);
          error(undefined1, lasterrcnt);
          return (ERROR);
        }
        resettkn(si);        /* to assure a clean restart */
        /* now fall through to try the control constructs */
        break;
  }

  resettkn(si);              /* reset in case EXPRESSION has used up tokens */

/* the following could be done by a case if reserved words are given a value.
   If the keyword is found then the corresponding parse routine is executed.
   Its result will be the result of this routine. */
  if (equalsymbol(nexttoken, r_IF))
    return (IF_EXPR(tree));

  if (equalsymbol(nexttoken, r_WHILE))
    return (WHILE_EXPR(tree));

  if (equalsymbol(nexttoken, r_FOR))
    return (FOR_EXPR(tree));

  if (equalsymbol(nexttoken, r_CASE))
    return (CASE_EXPR(tree));

  if (equalsymbol(nexttoken, r_REPEAT))
    return (REPEAT_EXPR(tree));

  if (equalsymbol(nexttoken, r_EXIT))
    return (EXIT_EXPR(tree));

  if (tokenprop == commentprop)
    return (COMMENT(tree));

  resettkn(si);              /* reset in case STMT_EXPR has used up tokens */
  return (FAIL);
}


/* ROUTINE NAME :   IX_ASSIGN_EXPR
                                           |------|
    indexed_assign_expr --->indexed_var--->| GETS |--->expr
                                           |------|
*/

static nialint
IX_ASSIGN_EXPR(nialptr * tree)
{
  nialptr     idtree,
              etree;
  int         res;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "   IX_ASSIGN_EXPR\n");
#endif

  res = ID(&idtree, passive);  /* look for an identifier */

  if (res == FAIL)
    return (res);

  if (tag(idtree) == t_identifier || get_entry(idtree) == notfound
      || get_role(idtree) != Rvar) { /* variable is defined */
    freeup(idtree);          
    return (FAIL);
  }
  res = IX_VAR(&idtree); /* attempt to parse an indexed variable */

  if (res != SUCCEED) {
    freeup(idtree);  /* has id tree from ID call */
    return (res);
  }
  if (!equalsymbol(nexttoken, r_GETS) &&
      !equalsymbol(nexttoken, r_ASSIGNSYM)) {
    freeup(idtree);          /* One can get here in an error condition. e.g.
                              * d|[,0,] h, where h is undefined. */
    error(incomplete_parse, 46);  /* 46 */
    return (ERROR);
  }

  accept1(); /* accept the assignment token */

  res = STMT_EXPR(&etree); /* attempt to parse a statement expression */

  if (res == SUCCEED) { /* build the parse tree and return success */
    *tree = b_indexedassign(idtree, etree);
    return (SUCCEED);
  }

  freeup(idtree);  /* frees the indexed variable tree */
  if (res == FAIL)
    error(exp_expr, 47);     /* 47 */ /* it's an error if no statement expression */

  return (ERROR);
}


/*
   ROUTINE NAME :   ASSIGN_EXPR

                                |------|
    assign_expr ---->idlist---->| GETS |---->expr--------------->
                                |------|
*/

static nialint
ASSIGN_EXPR(nialptr * tree)
{
  nialptr     t1,
              t2,
              id,
              var,
              newvar,
              sym;
  nialint     si;
  int         res,
              j,
              k,
              hit,
              nargs,
              role;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "   ASSIGN_EXPR \n");
#endif

  si = token_index;  /* remember token position */

  IDLIST(&t1, active); /* parse an identifier list */

  if (tally(t1) > 1) {       /* IDLIST may succeed with empty list */
    if (equalsymbol(nexttoken, r_GETS) ||
        equalsymbol(nexttoken, r_ASSIGNSYM)) {
      accept1(); /* accept the assign symbol */
      
      /* process the id list */
      nargs = tally(t1) - 1;
      for (j = 1; j <= nargs; j++) {
        var = fetch_array(t1, j); /* select identifier */
        if (tag(var) == t_identifier) { /* new identifier */
          id = get_id(var);
          sym = get_sym(var);
          if (symprop(sym) == stpclosed) { /* check that it is current symbol table */
            if (sym != (nialptr) fetch_array(current_env, 0)) {
              error(ass_nonlocal, 48);
              freeup(t1);
              return (ERROR);
            }                /* 48 */

            /* check that the id has not been referred to */
            hit = false;
            k = 0;
            while (!hit && k < tally(referred)) {
              if (id == fetch_array(referred, k))
                hit = true;
              k++;
            }
            if (hit) { /* the id has been referred to */
              error(ref_bfr_ass, 49);
              freeup(t1);
              return (ERROR);
            }                /* 49 */
          }

          /* assignment allowed, install the new variable */
          newvar = varinstall(id, Rvar, sym);
          if (symprop(sym) == stpclosed) {  /* record in local list */
            st_s_valu(get_entry(newvar), createint(localcnt));
            localcnt++;
          }
          replace_array(t1, (nialint) j, newvar);
        }
        else { /* the variable already exists */
          role = get_role(var);
          if (role != Rvar && role != Rident) { /* must be op or tr */
            error(invalid_assign, 50);  /* 50 */
            freeup(t1);
            return (ERROR);
          }

          if (role == Rident)
            st_s_role(get_entry(var), Rvar);  /* set role to variable */
        }
      }

      res = STMT_EXPR(&t2); /* attempt to parse a statement expression */

      if (res == SUCCEED) { /* build the parse tree and return success */
        *tree = b_assignexpr(t1, t2);
        return (SUCCEED);
      }

      freeup(t1); /* frees the id list parse tree */
      if (res == ERROR)
        return (res);
      
      /* res is FAIL, expecting a statement expression */
      error(exp_expr, 51);   /* 51 */
      return (ERROR);
    }
  }

  freeup(t1); /* empty idlist, so we clean up and fail */
  resettkn(si);
  return (FAIL);
}


/*
   ROUTINE NAME :  WHILE_EXPR

                   |-------|           |----|               |----------|
  loop_expr ---|-->| WHILE |--->expr-->| DO |--->expr_seq-->| ENDWHILE |-->
                   |-------|           |----|               |----------|

*/

static nialint
WHILE_EXPR(nialptr * tree)
{
  nialptr     t1,
              t2;
  int         res;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "WHILE_EXPR  \n");
#endif

  accept1();  /* accept the WHILE token */

  res = EXPRESSION(&t1); /* attempt to parse an expression */

  if (res == SUCCEED) {  /* look for DO */
    if (!equalsymbol(nexttoken, r_DO)) {
      error(exp_do, 52);     /* 52 */
      freeup(t1);
      return (ERROR);
    }
    
    accept1(); /* accept the DO token */
    loopcount++;
    t1 = st_exprseq(t1);     /* store expr as seq to assist debugging code */

    res = EXPR_SEQ(&t2); /* atempt to parse expression sequence */

    loopcount--;
    if (res == SUCCEED) { /* look for the ENDWHILE */
      if (!equalsymbol(nexttoken, r_ENDWHILE)) {
        error(exp_endwhile, 53);  /* 53 */
        freeup(t1);
        freeup(t2);
        return (ERROR);
      }

      accept1(); /* accept the ENDWHILE */
      /* build the parse tree and return with success */
      *tree = b_whileexpr(t1, t2);
      return (res);
    }

    freeup(t1);
    if (res == ERROR) {
      ite_cleanup(0);        /* use if-then-else cleanup routine to fix undefined */
      return (res);
    }

    /* res is FAIL, expecting an expression seq */
    error(exp_eseq, 54);     /* 54 */
    return (ERROR);
  }

  if (res == ERROR) {
    ite_cleanup(0);          /* use if-then-else cleanup routine to fix undefined */
    return (res);
  }

  /* no expression after WHILE, force an error */
  error(exp_expr, 56);       /* 56 */
  return (ERROR);
}


/*
  An EXIT expression is keyword EXIT followed by an expression.
*/

static nialint
EXIT_EXPR(nialptr * tree)
{
  nialptr     t1;
  int         res;

  accept1();  /* accept the EXIT token */

  if (loopcount <= 0) { /* must be in a loop */
    error(exit_context, 57); /* 57 */
    return (ERROR);
  }

  res = EXPRESSION(&t1); /* attempt to parse an expression */

  if (res == SUCCEED) { /* build the parse tree and return success */
    *tree = b_exitexpr(t1);
    return (SUCCEED);
  }

  if (res == ERROR) {
      ite_cleanup(0);        /* use if-then-else cleanup routine to fix undefined */
      return (ERROR);
  }

  /* res is FAIL, expecting an expression so error */
  error(exp_eseq, 59);     /* 59 */
  return (ERROR);
}



/* ROUTINE   REPEAT_EXPR

                   |--------|              |-------|          |-----------|
repeat_expr------->| REPEAT |-->expr_seq-->| UNTIL |-->expr-->| ENDREPEAT |-->
                   |--------|              |-------|          |-----------|
*/

static nialint
REPEAT_EXPR(nialptr * tree)
{
  nialptr     t1,
              t2;
  int         res;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "REPEAT_EXPR \n");
#endif

  accept1(); /* accept the REPEAT token */

  loopcount++;

  res = EXPR_SEQ(&t1); /* attempt to parse an expression sequence */

  loopcount--;
  if (res == SUCCEED) { /* look for the UNTIL token */
    if (!equalsymbol(nexttoken, r_UNTIL)) {
      error(exp_until, 60);  /* 60 */
      freeup(t1);
      return (ERROR);
    }

    accept1(); /* accept the UNTIL */

    res = EXPRESSION(&t2); /* attempt to parse an expression */

    if (res == SUCCEED) { /* look for the ENDREPEEAT */
      if (!equalsymbol(nexttoken, r_ENDREPEAT)) {
        error(exp_endrepeat, 61); /* 61 */
        freeup(t1);
        freeup(t2);
        return (ERROR);
      }

      accept1(); /* accept the ENDREPEAT */

      t2 = st_exprseq(t2);   /* store expr as a seq to assist debugging code */
      /* build the parse tree and return success */
      *tree = b_repeatexpr(t1, t2);
      return (res);
    }

    freeup(t1); /* free the exor seq tree */
    if (res == ERROR) {
      ite_cleanup(0);        /* use if-then-else cleanup routine to fix undefined */
      return (res);
    }

    /* expression failed, report an error */
    error(exp_expr, 63);     /* 63 */
    return (ERROR);
  }

  /* expr seq iattempt found an error */
  if (res == ERROR) {
    ite_cleanup(0);          /* use if-then-else cleanup routine to fix undefined */
    return (res);
  }

  /* exp seq failed, report an error */
  error(exp_eseq, 64);       /* 64 */
  return (ERROR);
}


/* ROUTINE    FOR_EXPR

            |-----|            |------|          |----|              |--------|
for_expr--->| FOR |-->idlist-->| WITH |-->expr-->| DO |-->expr_seq-->| ENDFOR |--->
            |-----|            |------|          |----|              |--------|
*/


static nialint
FOR_EXPR(nialptr * tree)
{
  nialptr     t1,
              t1b,
              t2,
              t3,
              var,
              newvar,
              id,
              sym;
  int         res,
              role,
              k,
              hit;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "   FOR_EXPR \n");
#endif
  accept1();  /* accept the FOR */

  res = ID(&t1b, active); /* attempt to parse a single identifier */

  if (res != SUCCEED) {
    error(exp_ident, 65);
    return (ERROR); /* 65 */
  }
  /* store the identifier in an idlist */
  t1 = st_idlist();
  t1 = nial_extend(t1, t1b);

  /* look for the WITH */
  if (!equalsymbol(nexttoken, r_WITH)) {
    error(exp_with, 66);     /* 66 */
    freeup(t1);
    return (ERROR);
  }

  accept1();  /* accept the WITH */
  var = fetch_array(t1, 1);  /* select identifier */

  if (tag(var) == t_identifier) {
    id = get_id(var);
    sym = get_sym(var);
    if (symprop(sym) == stpclosed) {  
      /* check that it is in current symbol table */
      if (sym != (nialptr) fetch_array(current_env, 0)) {
        error(ass_nonlocal, 67);
        freeup(t1);
        return (ERROR);  /* 67 */
      }                      
      /* check that id has not been referred to */
      hit = false;
      k = 0;
      while (!hit && k < tally(referred)) {
        if (id == fetch_array(referred, k))
          hit = true;
        k++;
      }
      if (hit) {
        error(ref_bfr_ass, 68);
        freeup(t1);
        return (ERROR); /* 68 */
      }                      
    }

    /* install the new variable */
    newvar = varinstall(id, Rvar, sym);
    if (symprop(sym) == stpclosed) {  /* record in local list */
      st_s_valu(get_entry(newvar), createint(localcnt));
      localcnt++;
    }
    replace_array(t1, 1, newvar);
  }
  else { /* the identifier has a role */
    role = get_role(var);
    /* check that the role is a variable or identifier */
    if (role != Rvar && role != Rident) {
      error(invalid_assign, 69);  /* 69 */
      freeup(t1);
      return (ERROR);
    }
    if (role == Rident)
      st_s_role(get_entry(var), Rvar);
  }

  res = EXPRESSION(&t2); /* attempt to parse an expression */

  if (res == SUCCEED) {
    t2 = st_exprseq(t2);     /* store as expr seq to assist debugging code */

    /* look for the DO */
    if (!equalsymbol(nexttoken, r_DO)) {
      error(exp_do, 70);     /* 70 */
      freeup(t1);
      freeup(t2);
      return (ERROR);
    }

    accept1(); /* accept the DO */

    loopcount++;

    res = EXPR_SEQ(&t3); /* attempt to parse an expression sequence */

    loopcount--;
    if (res == SUCCEED) {
      /* look for the ENDFOR */
      if (!equalsymbol(nexttoken, r_ENDFOR)) {
        error(exp_endfor, 71);  /* 71 */
        freeup(t1);
        freeup(t2);
        freeup(t3);
        return (ERROR);
      }

      accept1(); /* accept the ENDFOR */
      *tree = b_forexpr(t1, t2, t3);  /* build the parse tree */
      return (SUCCEED);
    }

    /* res is ERROR or FAIL, clean up */
    freeup(t1);
    freeup(t2);
    if (res == ERROR) {
      ite_cleanup(0);        /* use if-then-else cleanup routine to fix undefined */
      return (res);
    }

    /* EXPR_SEQ returned FAIL, report an error */
    error(exp_eseq, 72);     /* 72 */
    return (ERROR);
  }

  /* res is ERROR or FAIL, clean up */ 
  freeup(t1);
  if (res == ERROR) {
    ite_cleanup(0);          /* use if-then-else cleanup routine to fix undefined */
    return (res);
  }

 /* EXPRESSION returned FAIL, report an error */
  error(exp_expr, 74);       /* 74 */
  return (ERROR);
}


/* ROUTINE NAME :   IF_EXPR
                             if--elseif--else-endif
*/

static nialint
IF_EXPR(nialptr * tree)
{
  nialptr     t1;
  nialint     cnt;
  int         res;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "IF_EXPR \n");
#endif

  cnt = 0;
  accept1();  /* accept the IF */

  apush(createint(t_ifexpr)); /* start to build the parse tree */
  cnt++;

  res = EXPRESSION(&t1);  /* attempt to parse an expression */

  if (res != SUCCEED) {
    if (res == FAIL) {
      /* report an error */
      error(exp_expr, 75); /* 75 */
    }
    ite_cleanup(cnt); /* use if-then-else cleanup routine to fix undefined */
    return (ERROR);
  }

  /* found an expression */
  t1 = st_exprseq(t1);       /* store as expr seq to assist debugging code */
  apush(t1);  /* push it on the stack */
  cnt++;

  /* look for the THEN */
  if (!equalsymbol(nexttoken, r_THEN)) {
    error(exp_then, 77);     /* 77 */
    ite_cleanup(cnt); /* use if-then-else cleanup routine to fix undefined */
    return (ERROR);
  }

  accept1();  /* accept the THEN */

  res = EXPR_SEQ(&t1); /* attempt to parse an expression sequence */

  if (res != SUCCEED) {
    if (res == FAIL) {
      /* report an error */
      error(exp_eseq, 78);   /* 78 */
    }
    ite_cleanup(cnt); /* use if-then-else cleanup routine to fix undefined */
    return (ERROR);
  }

  /* found a then clause */
  apush(t1);  /* push the then clause */
  cnt++;

/* loop to parse indefinite number of ELSEIF <EXPR> THEN <EXPR_SEQ> */
  
while (equalsymbol(nexttoken, r_ELSEIF)) {
    accept1(); /* accept the ELSEIF */

    res = EXPRESSION(&t1); /* attempt to parse an expression */
    if (res != SUCCEED) {
      if (res == FAIL) { /* report an error */
        error(exp_expr, 79);
      }                      /* 79 */
      ite_cleanup(cnt); /* use if-then-else cleanup routine to fix undefined */
      return (ERROR);
    }

    /* found an expression, push it as expr seq */
    t1 = st_exprseq(t1);     /* to assist debugging code */
    apush(t1);
    cnt++;

    /* look for THEN */
    if (!equalsymbol(nexttoken, r_THEN)) {
      error(exp_then, 81);   /* 81 */
      ite_cleanup(cnt); /* use if-then-else cleanup routine to fix undefined */
      return (ERROR);
    }

    accept1(); /* accept the THEN */

    res = EXPR_SEQ(&t1); /* attempt to parse an expression sequence */
 
    if (res != SUCCEED) {
      if (res == FAIL)  /* report an error */
        error(exp_eseq, 82); /* 82 */
      ite_cleanup(cnt); /* use if-then-else cleanup routine to fix undefined */
      return (ERROR);
    }

    /* found an expression sequence */
    apush(t1);
    cnt++;
  }

   /* look for ELSE */
  if (equalsymbol(nexttoken, r_ELSE)) {
    accept1(); /* accept the ELSE */

    res = EXPR_SEQ(&t1); /* attempt to parse an expression sequence */
    if (res != SUCCEED) {
      if (res == FAIL) /* report an error */
        error(exp_eseq, 83); /* 83 */
      ite_cleanup(cnt); /* use if-then-else cleanup routine to fix undefined */
      return (ERROR);
    }

    /* found an expression sequence, push it */
    apush(t1);
    cnt++;
  }

  /* look for the ENDIF */
  if (!equalsymbol(nexttoken, r_ENDIF)) {
    error(exp_endif, 84);    /* 84 */
    ite_cleanup(cnt);
    return (ERROR);
  }
  
  accept1(); /* accept the ENDIF */
  *tree = b_ifexpr(cnt); /* build the parse tree using items in the stack */
  return (SUCCEED);
}

/* routine to clean up an indefinite number of expressions built up
  in an if-then-else that fails. If it fails because of an undefined
  then there is no error on the stack and it must be added after the
  cleanup.
*/

static void
ite_cleanup(int cnt)
{
  nialptr     err = Null,
              it;            /* err initialized to avoid gcc warning */
  int         i;

  if (lasterror != undefined)
    err = apop();
  for (i = 0; i < cnt; i++) {
    it = apop();
    freeup(it);
  }
  if (lasterror != undefined) {
    apush(err);
  }
  else
    error(undefined1, lasterrcnt);
}

/*
   ROUTINE NAME :   CASE_EXPR

            |------|        |------|             |---|              |-----|          |---------|
case_expr ->| CASE |->expr->| FROM |--^->const-->| : |--->expr_seq->| END |--|--|-^->| ENDCASE |----->
            |------|        |------|  |          |---|              |-----|  |  | |  |---------|
                                      |<-------------------------------------|  | |
                                                                                | |
                                      |<----------------------------------------| |
                                      |  |------|                   |-----|       |
                                      |->| ELSE |-------->expr_seq->| END |-------|
                                         |------|                   |-----|


*/


static nialint
CASE_EXPR(nialptr * tree)
{
  nialptr     t1,
              t2,
              label = Null,
              clist,
              slist,
              clistvals;
  int         res,
              cont;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "   CASE_EXPR \n");
#endif
  /* initialize the case lists */
  clist = Null;
  clistvals = Null;
  slist = Null;

  accept1();  /* accept the CASE token */

  res = EXPRESSION(&t1); /* attempt to parse an expression */

  if (res == SUCCEED) {
    /* look for From */
    if (!equalsymbol(nexttoken, r_FROM)) {
      error(exp_from, 85);   /* 85 */
      freeup(t1);
      return (ERROR);
    }

    accept1(); /* accept the FROM */
    t1 = st_exprseq(t1);     /* store as expr seq to assist debugging code */
    cont = true;

    /* loop looking for expression sequences preceded by a constant and ":" */
    while (cont) {
      /* look for the constant */
      if (tokenprop >= constprop) {
        label = b_constant(tokenprop - constprop, nexttoken);
        res = SUCCEED;
      }
      else
        res = FAIL;

      if (res == SUCCEED) {
        accept1(); /* accept the contant token and look for the COLON */
        if (!equalsymbol(nexttoken, r_COLON)) {
          error(exp_colon, 86); /* 86 */
          freeup(t1);
          freeup(label);
          freeup(clistvals);
          freeup(clist);
          freeup(slist);
          return (ERROR);
        }

        accept1(); /* accpet the COLON */

        res = EXPR_SEQ(&t2);  /* attempt to parse an epxression sequence */

        if (res == SUCCEED) {
          /* look for the END token */
          if (!equalsymbol(nexttoken, r_END)) {
            error(exp_end, 87); /* 87 */
            freeup(t1);
            freeup(t2);
            freeup(label);
            freeup(clistvals);
            freeup(clist);
            freeup(slist);
            return (ERROR);
          }

          accept1(); /* accept the COLON */

          /* update the list of cases */
          clist = nial_extend(clist, label);
          eval(label);
          clistvals = nial_extend(clistvals, apop());
          slist = nial_extend(slist, t2);
        }
        else {
          freeup(t1);
          freeup(label);
          freeup(clistvals);
          freeup(clist);
          freeup(slist);
          if (res == ERROR) {
            ite_cleanup(0);  /* use if-then-else cleanup routine to fix undefined */
            return (res);
          }
          /* res is FAIL, report an error */
          error(exp_eseq, 88);  /* 88 */
          return (ERROR);
        }
      }
      else
        cont = false;
    }  /* done the loop */

    /* look for an ELSE token */
    if (equalsymbol(nexttoken, r_ELSE)) {
      accept1(); /* accept the ELSE */

      res = EXPR_SEQ(&t2);  /* attempt to parse an expression sequence */
      if (res == SUCCEED)
        slist = nial_extend(slist, t2);
      else {
        freeup(t1);
        freeup(clist);
        freeup(slist);
        freeup(clistvals);
        if (res == ERROR) {
          ite_cleanup(0);    /* use if-then-else cleanup routine to fix undefined */
          return (res);
        }
        /* res is FAIL, report an error */
        error(exp_eseq, 89); /* 89 */
        return (ERROR);
      }
    }
    else  /* no ELSE clause */
      slist = nial_extend(slist, Nulltree); /* put in null case  */

    /* look for ENDCASE token */
    if (!equalsymbol(nexttoken, r_ENDCASE)) {
      error(exp_endcase, 90);/* 90 */
      freeup(t1);
      freeup(clist);
      freeup(slist);
      freeup(clistvals);
      return (ERROR);
    }

    accept1(); /* accept the ENDCASE, build the parse tree */
    *tree = b_caseexpr(t1, clistvals, clist, slist);
    return (SUCCEED);
  }

  if (res == ERROR) {
    ite_cleanup(0);          /* use if-then-else cleanup routine to fix undefined */
    return (res);
  }
  /* res is FAIL, report an error */
  error(exp_expr, 92);       /* 92 */
  return (ERROR);
}

/* a comment is a "%" followed by text up unyil a ";".
   It is scanned as a single token. 
*/

static nialint
COMMENT(nialptr * tree)
{
#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "  COMMENT \n");
#endif
  if (tokenprop == commentprop) {
    *tree = b_commentexpr(nexttoken);
    accept1(); /* accept the comment token */
    return (SUCCEED);
  }
  return (FAIL);
}

/*
   ROUTINE NAME :   IDLIST


  idlist -------|------->id------->|--------------->
                |                  |
                |                  |
                |<-----------------|

   parses a list of zero or more identifiers.
   Note that IDLIST always succeeds and therefore does
   not return a result.
*/

static void
IDLIST(nialptr * tree, int searchtype)
{
  nialptr     t1,
              t2;
  int         res;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "   IDLIST  \n");
#endif

  /* initialize an empty idlist */
  t1 = st_idlist();

  res = ID(&t2, searchtype); /* attempt to parse first identifier */

  /* loop while still finding identifiers */
  while (res == SUCCEED) {
    t1 = nial_extend(t1, t2); /* extend the list of identifiers */
    res = ID(&t2, searchtype); /* attempt to parse another identifier */
  }
  *tree = t1;  /* pass back the list */
  return;
}


/*
   ROUTINE NAME :   ID


  id---------------->identifier-------------------->


ID recognizes an identifier and searches for its occurrence
in the environment according to the search strategy indicated.
If searchtype is formal,  the identifier is just returned
in a t_identifier node. Otherwise lookup is called to search
according to searchtype. If the identifier is found  a
t_variable node is constructed. Otherwise a t_identifier
node is made which records the last symbol table searched.
The local scoping conventions are inherent in the search
strategy used by lookup (in symtab.c).

ID never produces an ERROR.
*/

static nialint
ID(nialptr * tree, int searchtype)
{
  nialptr     id,
              entr,
              sym;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "ID \n");
#endif
  /* look for an identifier token */

  if (tokenprop == identprop) {
    id = nexttoken;
    accept1(); /* accpet the token */

    if (searchtype == formal) /* do not do the lookup, just build an identifier node */
      *tree = b_identifier(Null, Null, id);

    else { /* attempt to lookup the identifier in the symbol tables */
      entr = lookup(id, &sym, searchtype);
      if (entr == notfound)
        *tree = b_identifier(sym, entr, id);
      else
        *tree = b_variable((nialint) sym, (nialint) entr);
    }
    return (SUCCEED);
  }
  else
    return (FAIL);
}


/* The routine varinstall places an identifier in a symbol table with a given role.
   It returns a b_variable node.
*/

static      nialptr
varinstall(nialptr id, int role, nialptr sym)
{
  nialptr     entr,
              tree,
              value = Null;
  int         sysflag = symprop(sym) == stpglobal && instartup;

  /* initialize the result tree based on role if not in startup */
  if (!instartup) {
    if (role == Rexpr)
      value = no_excode;
    else if (role == Roptn)
      value = no_opcode;
    else if (role == Rtrans)
      value = no_trcode;
    else if (role == Rvar)
      value = no_value;
    else if (role == Rident)
      value = no_value;
#ifdef DEBUG
    else {
      nprintf(OF_DEBUG, "funny role %d\n", role);
      value = no_value;
    }
#endif
  }
  else
    value = no_value;

  /* create a symbol table entry */
  entr = mkSymtabEntry(sym, id, role, value, sysflag);

  /* build the variable parse tree */
  tree = b_variable((nialint) sym, (nialint) entr);
  return (tree);
}


/*
    ROUTINE NAME  :   IX_VAR

  --->id------>@--->expr
           |
           --->@@-->expr
           |
           --->|--->expr
           |
           --->#--->expr


*/

static nialint
IX_VAR(nialptr * tree)
{
  nialptr     idtree,
              etree = -1; /* initialized to avoid compiler warning */
  int         res,
              sw;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "   IX_VAR \n");
#endif

  idtree = *tree;      /* tree already contains an ID of a variable or a definition */
  
  /* confirm that it is a variable */
  if (get_role(idtree) != Rvar)
    return (FAIL);

  /* parse the type of index symbol and record type in sw */

  if (equalsymbol(nexttoken, r_CAT)) {
    accept1(); /* accept first "@" */
    if (equalsymbol(nexttoken, r_CAT)) {
      accept1(); /* accept second "@" */
      sw = t_reachput; 
    }
    else
      sw = t_pickplace;
  }
  else if (equalsymbol(nexttoken, r_BAR)) {
    accept1(); /* accept the "|" token */
    sw = t_slice;
  }
  else if (equalsymbol(nexttoken, r_HASH)) {
    accept1(); /* accept the "#" token */
    sw = t_choose;
  }
  else
    return (FAIL);

  res = PRIMARY(&etree); /* attempt to parse a primary expression */

  if (res == SUCCEED) {
    switch (sw) { 
      /* build appropriate parse tree */
      case t_reachput:
          *tree = b_reachput(idtree, etree);
          break;
      case t_pickplace:
          *tree = b_pickplace(idtree, etree);
          break;
      case t_slice:
          *tree = b_slice(idtree, etree);
          break;
      case t_choose:
          *tree = b_choose_placeall(idtree, etree);
    }

    return (SUCCEED);
  }
  if (res == FAIL) { /* report an error */
    error(exp_expr, 93);
    return (ERROR);
  }                          /* 93 */
  return (res);
}


/*
    ROUTINE NAME  :   SCOPED_VAR

  --->id------>:--->id

*/

static nialint
SCOPED_VAR(nialptr * tree)
{
  nialptr     idftree,
              idvtree;
  int         res,
              role,
              valid_object,
              funentry;

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "   SCOPED_VAR \n");
#endif

  idftree = *tree;           /* tree already contains an ID  */
  role = get_role(idftree);

  /* role of first id must be appropriate for having a scope */
  if (role != Roptn && role != Rexpr && role != Rtrans)
    return (FAIL);

  /* look for the colon, FAIL if not found */
  if (!equalsymbol(nexttoken, r_COLON))
    return (FAIL);

  funentry = get_entry(idftree);
  if (sym_flag(funentry)) { /* id is a system name, report error */
    error(sysname_scope, 95);/* 95 */
    return (ERROR);
  }

  /* check the actual parse tree to be sure we've got the expected object */
  valid_object = ((tag(sym_valu(funentry)) == t_opform) ||
                  (tag(sym_valu(funentry)) == t_trform) ||
                  (tag(sym_valu(funentry)) == t_block));

  if (!valid_object) {
    error(no_scope, 96);     /* 96 */
    return (ERROR);
  }

  accept1();  /* accept the colon */

  res = ID(&idvtree, formal); /* attempt to parse the second id */

  if (res == SUCCEED) {
    *tree = b_scoped_var(idftree, idvtree);
    return (SUCCEED);
  }
  if (res == FAIL) { /* report an error */
    error(exp_var, 94);
    return (ERROR);
  }                          /* 94 */
  return (res);
}


#define EC 3                 /* amount of error context to display */


/* in DEBUG mode, the error no is displayed */

#ifdef DEBUG
static void
error(int messno, int errcnt)
#else
static void
proderror(int messno)
#endif
{
  char       *mess;
  char        eres[1000];    /* room for a very large error message */

#ifdef DEBUG
  if (debug)
    nprintf(OF_DEBUG, "  ERROR \n");
#endif

  lasterror = messno;
  if (lasterror == undefined) { /* defer building undefined errors */
    deferredsi = token_index;
#ifdef DEBUG
    lasterrcnt = errcnt;
#endif
    return;
  }

  /* select the appropriate error message text */
  switch (messno) {
    case incomplete_parse:
        mess = "tokens left: ";
        break;
    case exp_expr:
        mess = "expecting expression: ";
        break;
    case exp_optn:
        mess = "expecting operation: ";
        break;
    case exp_tr:
        mess = "expecting transformer: ";
        break;
    case exp_rparen:
        mess = "expecting right parenthesis: ";
        break;
    case exp_semicolon:
        mess = "expecting semicolon: ";
        break;
    case exp_then:
        mess = "expecting then: ";
        break;
    case exp_endif:
        mess = "expecting endif,elseif, or else: ";
        break;
    case exp_eseq:
        mess = "expecting expression sequence: ";
        break;
    case exp_end:
        mess = "expecting end: ";
        break;
    case exp_endwhile:
        mess = "expecting endwhile: ";
        break;
    case exp_do:
        mess = "expecting do: ";
        break;
    case exp_endrepeat:
        mess = "expecting endrepeat: ";
        break;
    case exp_until:
        mess = "expecting until: ";
        break;
    case exp_endfor:
        mess = "expecting endfor: ";
        break;
    case exp_with:
        mess = "expecting with: ";
        break;
    case exp_ident:
        mess = "expecting identifier: ";
        break;
    case exp_from:
        mess = "expecting from: ";
        break;
    case exp_colon:
        mess = "expecting colon: ";
        break;
    case exp_endcase:
        mess = "expecting constant or endcase: ";
        break;
    case is_defined:
        mess = "name already defined: ";
        break;
    case parse_fail:
        mess = "expecting an action: ";
        break;
    case exp_def:
        mess = "expecting a definition: ";
        break;
    case exp_basic:
        mess = "expecting a basic op: ";
        break;
    case exp_rbracket:
        mess = "expecting right bracket: ";
        break;
    case exp_aot:
        mess = "unexpected token or eol: ";
        break;
    case undefined1:
        mess = "undefined identifier: ";
        break;
    case exp_external:
        mess = "expecting external definition: ";
        break;
    case mismatch_ext:
        mess = "mismatched external specification: ";
        break;
    case exp_body:
        mess = "expecting body: ";
        break;
    case exp_rcurly:
        mess = "expecting end of block: ";
        break;
    case ill_null_form:
        mess = "illegal form: ";
        break;
    case init_complete:
        mess = "only during initialization: ";
        break;
    case ass_nonlocal:
        mess = "assignment to nonlocal closed block: ";
        break;
    case ref_bfr_ass:
        mess = "assignment to referenced name: ";
        break;
    case invalid_assign:
        mess = "invalid assignment: ";
        break;
    case exp_quotepgm:
        mess = "expecting quoted expression: ";
        break;
    case exp_primary:
        mess = "expecting a primary: ";
        break;
    case exp_stmtexpr:
        mess = "expecting a statement expression: ";
        break;
    case exit_context:
        mess = "exit used in wrong context: ";
        break;
    case inv_external:
        mess = "invalid use of external declaration: ";
        break;
    case parser_error:
        mess = "parser failed: ";
        break;
    case role_error:
        mess = "mismatched role in definition: ";
        break;
    case exp_defn:
        mess = "expecting a definition: ";
        break;
    case local_reused:
        mess = "local in nonlocal list: ";
        break;
    case arg_reused:
        mess = "argument in nonlocal list: ";
        break;
    case invalid_optn:
        mess = "invalid operation: ";
        break;
    case no_scope:
        mess = "No Scope: ";
        break;

    default:
        mess = "error msg error: ";
        break;
  }

  strcpy(eres, "?");  /* start with a "?" */
  strcat(eres, mess);

  /* the following block builds up the error context by adding
     tokens to the error string and placing <***> after the token 
     where the error is identified.
     It does this by building a token stream and then using descan
     to convert it into the context part of the error message */

  {
    int         start,
                end,
                j;

    start = (token_index - 2 * EC <= 0 ? 1 :
             token_index - 2 * EC);
    end = (token_index + 2 * EC - 1 >= tally(tokens) ?
           tally(tokens) - 1 : token_index + 2 * EC - 1);

    apush(createint(t_tokenstream));
    for (j = start; j < token_index; j++)
      apush(fetch_array(tokens, j));
    apush(createint(constphprop));
    apush(makephrase("<***>"));
    for (j = token_index; j <= end; j++)
      apush(fetch_array(tokens, j));
    mklist(end - start + 4); /* start to end plus 3 extras */

    idescan();
    ifirst();
    strcat(eres, pfirstchar(top));
    freeup(apop());
  }

#ifdef DEBUG
  { /* code to add the error no to the message */
    char        errcntstr[10];
    sprintf(errcntstr, " %d", errcnt);
    strcat(eres, errcntstr);
  }
#endif

  /* create the error message as a fault without triggering */
  apush(createatom(faulttype, eres));
}
