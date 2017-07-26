/* ==============================================================

   MODULE     SYMTAB.C

  COPYRIGHT NIAL Systems Limited  1983-2016


   This is the symbol table module.

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

/* STLIB */
#include <string.h>

/* SJLIB */
#include <setjmp.h>


/* Q'Nial header files */

#include "symtab.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"
#include "if.h"

#include "parse.h"           /* for parse tree node tags */
#include "roles.h"           /* for role codes */
#include "faults.h"          /* for Cipher */
#include "ops.h"             /* pair */
#include "blders.h"          /* builders and tag */
#include "getters.h"         /* for getters */
#include "fileio.h"          /* for STDOUT */
#include "utils.h"           /* getuppername */
#include "eval.h"            /* for d_clear_watches */

static nialptr enter_binary(nialptr entr, nialptr name);
static void eraseSymtabEntry(nialptr entr);
static nialptr create_entry(void);
static int  checkentry(nialptr entr);
static void Olookup(nialptr entr, int sysnames);

static nialint defcnt;

/*             Symbol Table Routines

 The symbol table mechanism is a collection of binary trees one for
 each separate naming scope created in the workspace. The system is
 given an initial environment consisting of a single binary tree
 containing the reserved words and the names of the predefined
 arrays, operations and transformers. This binary tree is called the
 global_symtab and every environment has access to it. As the
 user defines objects and does assignments the global_symtab grows.

 A separate binary tree is created for every naming scope as they
 are encountered by the parser. At all times current_env contains the
 list of symbol tables that form the static nesting of environments.
 The global and system symbol tables are held separately to reduce overhead.
 The current environment for each construct that defines a new symbol table
 is saved in the parse tree entry for the construct so that
 they can be reestablished at runtime.

 A symbol table is a triple consisting of a root, a current stack
 pointer and a property. It is named by the address where this triple occurs.
 This is assumed to remain constant. 

 The property of a symbol table is global, parameter, open or closed. 

 The local symbol table of an operation form whose body is a block is closed, 
 otherwise it is open. 

 The symbol table of a block is closed. 
 The symbol table of a transformer form is of type parameter.  
 The open vs closed information is used to enforce the static nesting rules.

 The stack pointer of a symbol table points to the current activation record
 associated with that symbol table at execution time if any. 
 A global symbol table entry do not have an activation record since its value 
 cell is in the entry itself. For other symbol tables the corresponding field 
 in the entry holds the offset to the value cell in the stack area for that
 entry.


 Each entry has 6 fields:

               +------+------+------+------+------+------+
               | name | role | valu | left | rght | flag |
               +------+------+------+------+------+------+

 name   The printvalue of the symbol stored as a phrase.

 role   The semantic meaning of the name. Either reserved, identifier,
 variable, constant array, operation, or transformer.

 valu   The value of the object if the entry is in the global symtab.
 For a variable it is the array value, for an expression, operation 
 or transformer it is the parse tree that represents it. For a local 
 symbol table this field contains the offset for the value cell in the local 
 area on the stack.

 left   The left child of the node in the binary tree.

 rght   The right child of the node in the binary tree.

 trflag This is patched into the role field to indicate that the
 object should be traced during evaluation.

 flag   Used for global symbols to indicate whether the symbol
 is system or user defined.


 A symbol table has four fields
     - the root of the binary tree (set to grounded on creation)
     - a pointer to the current activation record in the stack
       for local environments (set to -1 on creation)
     - a property field that indicates whether the symbol table
       is global or local, and if it is local whether it is open, 
       closed or a parameter table
     - a name indicating the owner of the table. It is either "GLOBAL"
       or the name of the definition holding the symbol table. An opform
       or block that is not named has name ANONYMOUS.
*/

/* routine to create a symbol table */

nialptr
addsymtab(int prop, char * stname)
{
  return (mkaquad(grounded, createint(-1), createint((nialint) prop), 
     makephrase(stname)));
}

/* routine to create a symbol table entry */

nialptr
mkSymtabEntry(nialptr sym, nialptr name, nialptr role, nialptr value, int sysflag)
{
  nialptr     entr;

  if (get_root(sym) == grounded) {  /* add new entry at the root */
    entr = create_entry();
    store_root(sym, entr);
  }
  else                       /* find place to enter it */
    entr = enter_binary(get_root(sym), name);
  /* store information on entry */
  st_s_name(entr, name);
  st_s_role(entr, role);
  st_s_valu(entr, value);
  st_s_flag(entr, sysflag);
  return (entr);
}

/* routine that implements the primitive operation erase that
   undefines a name. This routine has two restrictions:
   - it does not erase local names
   - the erased object is replaced by an "undefined" object
   of the same class (array, op, or tr)
   */

void
ierase()
{
  nialptr     name,
              entr,
              sym;
  /* check if a valid name */
  name = getuppername();
  if (name == grounded) {
    apush(makefault("?invalid name in erase"));
  }

  else {
    entr = lookup(name, &sym, passive);  /* try to look up the name */
    if (entr != notfound) {
      if (sym_flag(entr)) {
        apush(makefault("?system name in erase"));
      }

      else { /* check that the name is global */
        if (sym == global_symtab) {
          if (sym_role(entr) != Rident) { /* already erased */
            st_s_trflg(entr, 0);  /* clear the trace flag */
            st_s_brflg(entr, 0);  /* clear the break flag */
            if (sym_role(entr) == Rvar)
              d_clear_watches(sym, entr); /* clear a watch on the global
                                           * variable */
            else {           /* find which local symtab this defn has */
              nialptr     dfn_body = sym_valu(entr);

              if (tag(dfn_body) == t_expression || tag(dfn_body) == t_opform ||
                  tag(dfn_body) == t_trform) {  /* has a scope */
                nialptr     dfn_sym = get_trsym(sym_valu(entr));  
                            /* the symtab for the defn */

                d_clear_watches(dfn_sym, entr); /* clear all watches on the
                                                   local variables of the dfn */
              }
            }
            
            d_remove_break(name); /* remove a break on the name */
            eraseSymtabEntry(entr); /* erase the entry */
          }
          apush(Nullexpr);
        }

        else
          apush(makefault("?local name in erase"));
      }
    }

    else
      apush(makefault("?unknown name in erase"));
  }
  freeup(name);
}

/* internal routine to do the erasure */

static void
eraseSymtabEntry(nialptr entr)
{
  int         role;

  /* make old entry the undefined, operation, or transformer, and freeup the
   * old value and definition. */
  role = sym_role(entr);
  switch (role) {
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
}

/* internal routine that does the binary search on insertion.
   key used to decide left or right is the token name */

static      nialptr
enter_binary(nialptr entr, nialptr name)
{
  nialptr     z;

  if (strcmp(pfirstchar(sym_name(entr)), pfirstchar(name)) >= 0)
  { /* current entry is after the new name, use left side */
    if (sym_left(entr) == grounded) 
    { /* create new entry on the left */
      z = create_entry();
      st_s_left(entr, z);
      return (z);
    }

    else /* continue the search down the left node recursively */
      return (enter_binary(sym_left(entr), name));
  }
  else 

  /* current entry is before the new name, use right side */
  if (sym_rght(entr) == grounded) 
  { /* create new entry on the right */
    z = create_entry();
    st_s_rght(entr, z);
    return (z);
  }

  else /* continue the search down the right node recursively */
    return (enter_binary(sym_rght(entr), name));
}

/* routine to create a symbol table entry. All its fields are
   set to grounded. */

static      nialptr
create_entry()
{
  nialptr     entr;
  nialint     i;
  nialint     sz = sym_entrysize;

  entr = new_create_array(atype, 1, 0, &sz);
  for (i = 0; i < sym_entrysize; i++)
    store_array(entr, i, grounded);
  return (entr);
}

/* binary tree lookup routine */

nialptr
Blookup(nialptr entr, nialptr name)
{
  if (entr == grounded)      /* not a node */
    return (notfound);

  if ((int) entr == -1) {  /* error condition */
    exit_cover1("invalid entry in symbol table", NC_FATAL);
    return (notfound);       /* fake return */
  }

  if (sym_name(entr) == name)/* found the entry, return it */
    return (entr);

  /* decide on direction based on comparing the strings */
  if (strcmp(pfirstchar(sym_name(entr)), pfirstchar(name)) >= 0)
    return (Blookup(sym_left(entr), name)); /* search on the left */
  else
    return (Blookup(sym_rght(entr), name)); /* search on the right */
}

/*  lookup routine: general symbol table lookup routine.
    This routine is primarily designed to meet the needs of
    scope handling for the parser. It implies much of the semantics
    of nested scopes. When called from parse() the searchtype
    can be statics, dynamic, active, passive, or nonlocal.

    For almost all calls outside of parse the searchtype is passive. The
    exception is ibreakin, which explicitly calls it with globals.

    A passive search simply looks through the symbol tables of the current
    environment and the global environment until a match is found.

    An active search implies the use of the name on the left of
    assignment or as the variable in a FOR_EXPR. The search continues
    through local symbol tables until a match is found unless the
    first local symbol table is closed.  If a local
    match isn't found the global symbol table is searched.
    (The closed rule detects that an attempt is being made to
     assign to a name that is not in the local scope.)

    A statics search is used when the name is being used for a definition
    in a block. The search is restricted to the current local symbol table.
    (statics is spelled with the trailing s for search to avoid conflict
     with the C keyword static).

    A statics search is used when the name is being used for a definition
    in the global environment or one being made dynamically from within
    a local environment. In this case the local environment is checked
    first then the global environment.

    A nonlocal search skips the first local symbol table. It is used to
    find variables declared NONLOCAL in a block.

    The result of lookup is the entry number if found or the signal
    "notfound" otherwise. The last symbol table searched is passed
    back in *sym.
*/

nialptr
lookup(nialptr name, nialptr * sym, int searchtype)
{
  int         found = false;
  nialptr     entr = notfound;  /* assume that the entry is not found */

  if (current_env != Null && searchtype != globals) { 
     /* there are local symbol tables */
    if (searchtype != nonlocal) { 
      /* look in first local symbol table */
      int         prop;

      *sym = fetch_array(current_env, 0); /* first local env */
      prop = symprop(*sym);
      if (prop == stpclosed) {  /* check nonlocal list */
        nialint     i = 1,   /* avoiding the tag */
                    tnlocs = tally(nonlocs);

        while (!found && i < tnlocs) {
          nialptr     id,
                      var = fetch_array(nonlocs, i);

          if (tag(var) == t_identifier)
            id = get_id(var);
          else
            id = get_var_pv(var);
          found = name == id; /* test assumes unique phrase representation */
          i++;
        }
      }

      if (!found) { /* not in the local list, search the tree */
        entr = Blookup(get_root(*sym), name);
        if (entr != notfound || searchtype == statics ||
            (searchtype == active && prop == stpclosed))
          return (entr);
      }
    }

    /* look in remaining local symbol tables unless dynamic */
    if (searchtype != dynamic) {
      nialint     i = 1,
                  tce = tally(current_env);
      /* loop over the environments */
      while (entr == notfound && i < tce) {
        *sym = fetch_array(current_env, i);
        entr = Blookup(get_root(*sym), name);
        i++;
      }
    }
  }
  if (entr == notfound) {    /* look in the global symbol table */
    *sym = global_symtab;
    entr = Blookup(get_root(*sym), name);
  }
  return (entr);
}


/* routine that implements the primitive symbols that gets
   all symbol table entries with their name and role.
   If arg is 0 just user's symbols, otherwise all of them.
   The symbols are pushed on the stack and counted in defcnt.
*/

void
isymbols()
{
  nialptr     x = apop();

  if (kind(x) != inttype) {
    apush(makefault("?symbols arg must be integer"));
  }
  else {
    int         sysnames = intval(x) == 1;

    defcnt = 0;
    Olookup(get_root(global_symtab), sysnames);
    if (defcnt != 0)
      mklist(defcnt);
    else
      apush(Null);
  }
  freeup(x);
}

/* routine to walk the global symbol table and
   store all requested entries with their name and role.
   Uses the global defcnt to keep track of the number created.
*/

static void
Olookup(nialptr entr, int sysnames)
{
  if (entr == grounded)
    return;
  if (!sym_flag(entr) || sysnames) {
    nialptr     r,
                osn = sym_name(entr);
    nialint     osr = sym_role(entr);

    switch (osr) {
      case Rres:
          r = makephrase("res");
          break;
      case Rident:
          r = makephrase("ident");
          break;
      case Rvar:
          r = makephrase("var");
          break;
      case Rexpr:
          r = makephrase("expr");
          break;
      case Rconst:
          r = makephrase("const");
          break;
      case Roptn:
          r = makephrase("op");
          break;
      case Rtrans:
          r = makephrase("tr");
          break;
      default:
          r = Cipher;
          break;
    }
    pair(osn, r);
    defcnt++;
  }
  Olookup(sym_left(entr), sysnames);
  Olookup(sym_rght(entr), sysnames);
}




/* routine that implements the primitive see that displays
   a definition using deparse, descan, etc.   */

void
isee()
{
  igetdef();                 /* getdef returns Cipher if bad name */
  if (kind(top) != faulttype) { /* an error message fault on stack. */
    nialptr     def;
    nialint     i;

    ideparse();
    idescan();
    def = apop();   /* this is a list of character strings */

    /* loop over the strings displaying them in order */
    for (i = 0; i < tally(def); i++) {
      nialptr     it = fetch_array(def, i);

      writechars(STDOUT, pfirstchar(it), tally(it), true);
    }
    /* result is ?noexpr */
    apush(Nullexpr);
    freeup(def);
  }
}

/* routine to check that an entry exists and is not a system name */
static int
checkentry(nialptr entr)
{
  if (entr == notfound) {
    buildfault("no such symbol");
    return (false);
  }
  else if (sym_flag(entr)) {
    buildfault("system name");
    return (false);
  }
  else
    return (true);
}


/* routine that implements the Nial primitive getdef which
   gets a definition out of the symbol table. Used by isee.
*/

void
igetdef()
{
  nialptr     sym,
              entr,
              definition = Null,
              pt,
              name,
              idlist;
  int         role;

  name = getuppername();
  if (name == grounded) {
    buildfault("bad arg to getdef");
    return;
  }

  /* look up the name and validate it */
  entr = lookup(name, &sym, passive);
  if (!checkentry(entr)) {
    freeup(name);
    return;                  /* checkentry pushes a fault */
  }

  /* check that it is global */
  if (sym != global_symtab)
  { apush(makefault("?cannot see a local definition"));
    freeup(name);
    return;
  }

  role = sym_role(entr);
  switch (role) {
    case Rident:
        buildfault("no such symbol");
        return;
    case Rvar:
        buildfault("getdef not possible for a variable");
        return;
    case Rexpr:
    case Roptn:
    case Rtrans:
        definition = sym_valu(entr);
        break;
  }

  /* have a parse tree for the definition value. Construct the
     parse tree for the corresponding definition */
  idlist = nial_extend(st_idlist(), b_variable((nialint) sym, (nialint) entr));
  definition = b_definition(idlist, definition, (role != Rconst));
  pt = b_parsetree(definition);
  apush(pt);
}
