/* ==============================================================

   MODULE     PROFILE.C

  COPYRIGHT NIAL Systems Limited  1983-2016


   This module does all the profiling work for Nial definitions.
   It interfaces closely with eval.c.


================================================================*/

/* Q'Nial file that selects features */

#include "switches.h"

#ifdef PROFILE               /* ( */

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

/* MALLOCLIB */
#ifdef OSX
#include <stdlib.h>
#elif LINUX
#include <malloc.h>
#endif

/* CLIB */
#include <ctype.h>

/* TIMELIB */

#ifdef TIMEHOOK

#include <sys/types.h>
#ifdef UNIXSYS
#include <sys/times.h>
#endif
#ifdef WINNIAL
#include <sys/time.h>
#endif
#include <sys/param.h>

#endif

/* Q'Nial header files */

#include "profile.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"
#include "if.h"

#include "symtab.h"          /* for symtab macros */
#include "getters.h"         /* for get macros */
#include "blders.h"          /* for get routines tag */
#include "parse.h"           /* for tag t_trform */
#include "utils.h"            /* for slower */
#include "roles.h"           /* for role codes */
#include "fileio.h"          /* for nprintf */


/* The profiler's work is done by building two data structures:
   a profiler symbol table of all defined names is built with fields for
   - entry,   its location in the Nial symbol table
   - name,  name associated with entry, used with transformer args
   - used,       a flag indicated it has been executed during profiling
   - num_locs  the number of direct call locations
   - size, the number of spaces for locations in the entry
   - locations, an array of pointers to direct call nodes
   - num_rcalls  the number of recursive calls made on this symbol

   a tree of nodes representing the dynamic call tree, where each node
   corresponds to a definition that has been called during
   profiling and its children are the definitions that it
   has called directly. The nodes contain:
   - opid,  the entry no for the definition in the Nial symbol table.
   - total_calls,  the number of times the definition has been entered
   - start_time, time when current call was initiated
   - end_time, time when the current call was finished
   - total_time, time for all completed calls so far
   - num_children, number of definitions it calls
   - num_spaces, length of space for children nodes
   - children, array of pointers to nodes for definitions it calls
   - parent, pointer to parent node

   The symtab list is initialized when profiling is turned on for
   the first time.
   As definitions are called the dynamic call tree is constructed.
   Call nodes in the tree are reused if they occur in the same sequence, adding
   times and increasing the call count. Profiling can be turned on or off
   several times.

   When th eoperation profile is called, the graph is traversed building up for
   each symbol lists of locations in the call tree where it has been
   called.
   Then the list of symbols in walked over, adding up the times and merging
   uses that occured in diferent places. The output is produced during this
   sweep and placed in file nial.prf or an alternate name giving by using
   setprofname.

   */

/* Call Tree node */

struct node {
  int         opid;          /* id number for the operation */
  int         total_calls;
  double      start_time;
  double      end_time;
  double      total_time;
  int         num_children;
  int         num_spaces;
  struct node **children;
  struct node *parent;
};


/* Symbol Table entry */

struct sym_tab_entry {
  int         id;
  char       *name;
  int         used;
  int         num_locs;
  int         size;
  int         toplevel_call;
  struct node **locations;   /* direct call locations in the calltree */
  int         num_rcalls;
};

typedef struct sym_tab_entry symentry;
static symentry **symtab = NULL;

static int  symtabsize = 0;  /* holds symbol table size used */
static int  symspace = 0;    /* holds size of array available for the symtab */

/* swplace is used to keep track of highest used symbol table entry.
   When a new entry is found it is swapped with the next available
   symbol entry which puts the symtab into the order in which symbols
   are encountered in the tree traversal. */

static int  swplace = 0;

static char trname[80];      /* used to pass tr name to its parameters */


static int  newprofile = true;  /* flag to warn that profiling is under way */

static int  traversed = false;  /* we don't want to traverse the call tree
                                   with traverse_tree more than once with
                                   calling clear_profiler. This can happen
                                   because iprofiletable does not call
                                   clear_profiler, and could be called
                                   multiple times.  But we only want
                                   traverse_tree called once (per profile
                                   session ) */

static struct node *calltree = NULL;  /* the root of the call tree */

static struct node *current_node = NULL;  /* the node for the definition
                                             being executed. */



static void build_symbol_table();
static void profilelookup(nialptr sym, nialptr entr, int sysnames);
static void make_sym_entry(int oid, char *name);
static void free_symtab();
static char *num_to_name(int num);
static struct node * make_node();
static int  find_child(struct node * n, int id);
static void free_tree(struct node * parent);
static void traverse_tree(struct node * parent);
static char *padright(int dist, char *ins);
static int  inlist(struct node ** list, int used, struct node * entry, int *pos);
static struct node **merge_children(symentry * in, int *used);
static void free_merge_list(struct node ** list, int size);
static nialptr tree_to_array(struct node *);



#define profile_time get_cputime

/* ============== profiler symbol table support routines ===========*/

/* routine to build the profiler symbol table from the information in
   the Nial symbol table */

static void
build_symbol_table()
{                      /* initialize the symtab array */
  if (!symtab) {
    symtab = (symentry **)
    malloc(SYMLISTSIZE * sizeof(symentry *));
    if (symtab == NULL) {
      exit_cover1( "Could not allocate space in profile code", NC_WARNING);
    }
    symtabsize = 0;
    symspace = SYMLISTSIZE;
    /* call the recursive routine to sweep the Nial symbol table */
    profilelookup(global_symtab, get_root(global_symtab), 0);
  }
}


/* routine to walk Nial symbol table to select definitions.
   Corresponding entries are made in the profiling symtab. */

static void
profilelookup(nialptr sym, nialptr entr, int sysnames)
{
  char       *tmp;

  if (entr == grounded)
    return;
  if (!sym_flag(entr) || sysnames) {
    nialptr     osn = sym_name(entr);
    int     osr = sym_role(entr);

    switch (osr) {
      case Rexpr:
          make_sym_entry(entr, slower(pfirstchar(osn)));
          break;
      case Roptn:
          if (sym == global_symtab)
            make_sym_entry(entr, slower(pfirstchar(osn)));
          else
            /* a formal parameter to a trform */
          {
            char        temp[80];

            strcpy(temp, trname);
            strcat(temp, ".");
            strcat(temp, tmp = slower(pfirstchar(osn)));
            make_sym_entry(entr, strdup(temp));
            free(tmp);
          }
          break;
      case Rtrans:
          {
            nialptr     tval = sym_valu(entr),
                        localsym;

            strcpy(trname, tmp = slower(pfirstchar(osn)));
            free(tmp);
            make_sym_entry(entr, strdup(trname)); 
              /* want to write an entry for the formal parameters, 
                 so search the local symbol table for the trform. */
            if (tag(tval) == t_trform) {
              localsym = get_trsym(tval);
              profilelookup(localsym, get_root(localsym), false);
            }
          }
          break;
      default:
          break;
    }
  }

  profilelookup(sym, sym_left(entr), sysnames);
  profilelookup(sym, sym_rght(entr), sysnames);
}

/* routine to make a symbol table entry and place it in the
   symtab array. If the latter is out of space it is expanded.
   No space is allocated for locations in the entry until they
   are needed.
*/

static void
make_sym_entry(int oid, char *name)
{                            /* allocate space for the entry */
  symentry   *newsym = (symentry *) malloc(sizeof(symentry));

  if (newsym == NULL) {
    exit_cover1( "Could not allocate space in profile code", NC_WARNING);
  }
  /* initialize it */
  newsym->id = oid;
  newsym->name = name;
  newsym->used = false;
  newsym->toplevel_call = false;
  newsym->num_locs = 0;
  newsym->size = 0;
  newsym->locations = NULL;
  newsym->num_rcalls = 0;
  /* expand symtab array if necessary */
  if (symtabsize >= symspace) {
    symentry  **newsym = (symentry **)
    realloc(symtab, (symspace + SYMLISTSIZE) * sizeof(symentry *));

    if (newsym == NULL) {
      exit_cover1( "Could not allocate space in profile code", NC_WARNING);
    }
    symtab = newsym;
    symspace += SYMLISTSIZE;
  }
  /* store the new sym */
  symtab[symtabsize] = newsym;
  symtabsize++;
}

/* routine to freeup the symtab structure used in the profiler */

static void
free_symtab()
{
  int         c;

  if (!symtab) return;

  for (c = 0; c < symtabsize; c++) {
    /* int i; */

    free(symtab[c]->name);
    /* for (i=0;i<symtab[c]->num_locs;i++) free(symtab[c]->locations[i]);  */
    free(symtab[c]->locations);
    free(symtab[c]);
  }
  free(symtab);
}

/* The routine num_to_name will search the symbol table for a particular
   operation "id" and return the string name of the operation.
   Since we construct special names for tranformer args, this
   must be used.  
*/

static char *
num_to_name(int num)
{
  int         ct;

  for (ct = 0; ct < symtabsize; ct++)
    if (symtab[ct]->id == num)
      return (symtab[ct]->name);
  return ("???");
}

/*============== call tree support routines ==================*/

/* routine to make and inititalize a new call tree node */

static struct node *
make_node()
{
  struct node *new_node = (struct node *) malloc(sizeof(struct node));

  if (new_node == NULL) {
    exit_cover1( "Could not allocate space in profile code", NC_WARNING);
  }
  new_node->opid = 0;
  new_node->total_calls = 0;
  new_node->start_time = 0;
  new_node->end_time = 0;
  new_node->total_time = 0;
  new_node->num_children = 0;
  new_node->num_spaces = 0;
  new_node->children = NULL;
  new_node->parent = NULL;
  return (new_node);
}


/* The routine find_child examines the immediate children of the node "n".
   if the operation "id" is already a child of the node "n", the
   location in the child list is returned.
   Otherwise the a new child node is allocated and the location of
   that new child node is returned. If there are no available spaces
   the children list is expanded by NODESIZE.
*/

static int
find_child(struct node * n, int id)
{
  int         cntr,
              res = -1,
              found = false;

  for (cntr = 0; cntr < n->num_children; cntr++) {
    if (n->children[cntr]->opid == id) {
      res = cntr;
      found = true;
    }
  }
  if (!found) {
    struct node *new_node;

    if (n->num_children >= n->num_spaces) { /* additional space needed */
      struct node **newchlist;

      if (n->children == NULL)
        newchlist = (struct node **) malloc(NODESIZE * sizeof(struct node *));
      else
        newchlist = (struct node **)
          realloc(n->children,
                  (sizeof(struct node *) * (n->num_spaces + NODESIZE)));

      if (newchlist == NULL) {
        exit_cover1( "Could not allocate space in profile code", NC_WARNING);
      }
      n->children = newchlist;
      n->num_spaces += NODESIZE;
    }
    new_node = make_node();
    n->children[n->num_children] = new_node;
    set_opid(new_node, id);
    set_parent(new_node, n);
    res = n->num_children;
    n->num_children++;
  }
  return (res);
}

/* Recursive free function for the call tree */

static void
free_tree(struct node * parent)
{
  int         c2;

  if (parent != NULL) {
    for (c2 = 0; c2 < parent->num_children; c2++) {
      free_tree(parent->children[c2]);
      free(parent->children[c2]);
    }
    free(parent->children);
  }
  return;
}


/* ==== routines used in analyzing the data and gathering results ========*/

/* This function traverses the call tree, and builds a symbol
   table.  Information is the symbol table is totalled.
*/

static void
traverse_tree(struct node * parent)
{
  int         c,
              c1,
              c2;
  int         sw = false;
  struct node *current;

  for (c2 = 0; c2 < parent->num_children; c2++) {
    current = parent->children[c2];
    /* nprintf(OF_DEBUG,"traversing node %p\n",current); */
    /* find the entry in symtab */
    c1 = 0;
    while (c1 < symtabsize && symtab[c1]->id != current->opid)
      c1++;
    if (c1 == symtabsize) {  /* current->opid is an unknown symbol. probably
                                due to definitions being added after
                                profiling turned on the first time. */
      nprintf(OF_MESSAGE, "unknown symbol encountered in profiling\n");
      break;
    }
    /* if the parent of this node is the top level of the call tree,
       then this is a top level call */
    if (parent == calltree)
      symtab[c1]->toplevel_call = true;
    /* nprintf(OF_DEBUG,"name is %s at %d  parent node is
       %p\n",symtab[c1]->name,c1,current->parent); */
    if (symtab[c1]->used == false)
      sw = true;
    symtab[c1]->used = true;

    /* find out if it is a recursive call */
    if (current->opid == parent->opid)  /* count recursive calls */
      symtab[c1]->num_rcalls += current->total_calls;
   
    
      if (symtab[c1]->num_locs >= symtab[c1]->size) {
        struct node **newlist;

        if (symtab[c1]->locations == NULL)
          newlist = (struct node **)
            malloc(LOCLISTSIZE * sizeof(struct node *));
        else
          newlist = (struct node **)
            realloc(symtab[c1]->locations,
                  (symtab[c1]->size + LOCLISTSIZE) * sizeof(struct node *));
        if (newlist == NULL) {
          exit_cover1( "Could not allocate space in profile code", NC_WARNING);
        }
        symtab[c1]->locations = newlist;
        symtab[c1]->size += LOCLISTSIZE;
      }

      symtab[c1]->locations[symtab[c1]->num_locs] = current;
      /* nprintf(OF_DEBUG,"node placed in locs at %d\n",symtab[c1]->num_locs); */
      symtab[c1]->num_locs++;
    
    if (sw) {      /* swap newly used item forward in the symtab array */
      symentry   *tmp = symtab[swplace];

      symtab[swplace] = symtab[c1];
      symtab[c1] = tmp;
      /* nprintf(OF_DEBUG,"entry swapped to %d\n",swplace); */
      swplace++;
    }
  }
  /* traverse children */
  for (c = 0; c < parent->num_children; c++)
    traverse_tree(parent->children[c]);
}


/* pad right returns a modified string that is "dist"
   wide.  The original string is free'd and a new string
   is malloc'd.
*/

static char *
padright(int dist, char *instring)
{
  int         c,
              diff;
  char       *tmp = (char *) malloc((dist * sizeof(char)) + 1);

  if (tmp == NULL)
    exit_cover1( "Could not allocate space in profile code", NC_WARNING);

  strncpy(tmp, instring, dist);
  if (strlen(instring) < dist) {
    diff = dist - strlen(instring);
    for (c = 0; c < diff; c++)
      strcat(tmp, ".");
    return tmp;
  }
  return (tmp);
}


/* inlist tests if "entry" is in the list of nodes "list". 
   If so it gives the position in pos */

static int
inlist(struct node ** list, int used, struct node * entry, int *pos)
{
  int         c = 0;

  while (c < used) {
    if (entry->opid == list[c]->opid) {
      *pos = c;
      return 1;
    }
    c++;
  }
  return 0;
}


/* merge_children merges the call information stored in the
   symbol table (for a particular entry).  This is because
   an operation may be called from all over the call tree
   and the time/calls data needs to be merged.  */


static struct node **
merge_children(symentry * in, int *used)
{
  int         c1,
              c2,
              listsize;
  int         nextpos = 0;
  struct node **tmp = (struct node **) malloc(NODESIZE * sizeof(struct node *));

  if (tmp == NULL) {
    exit_cover1( "Could not allocate space in profile code", NC_WARNING);
  }
  listsize = NODESIZE;
  *used = 0;
  for (c1 = 0; c1 < in->num_locs; c1++) {
    /* nprintf(OF_DEBUG,"c1 %d merging entry %s %d num_locs %d\n",
     c1,in->name, in->id,in->num_locs); */
    for (c2 = 0; c2 < in->locations[c1]->num_children; c2++) {
      struct node *curnode = in->locations[c1]->children[c2];
      int         pos;

      if (!inlist(tmp, *used, curnode, &pos)) {
        if (nextpos >= listsize) {
          struct node **newtmp =
          newtmp = (struct node **)
          realloc(tmp, (listsize + NODESIZE) * sizeof(struct node *));

          if (newtmp == NULL) {
            exit_cover1( "Could not allocate space in profile code", NC_WARNING);
          }
          tmp = newtmp;
          listsize += NODESIZE;
        }
        tmp[nextpos] = make_node();
        set_opid(tmp[nextpos], curnode->opid);
        pos = nextpos;
        nextpos++;
        *used += 1;
      }
      /* nprintf(OF_DEBUG,"child no %d curnode %p, parent
       %p\n",c2,curnode,curnode->parent); */
      set_total_time(tmp[pos],
                     curnode->total_time + tmp[pos]->total_time);
      set_total_calls(tmp[pos],
                      curnode->total_calls + tmp[pos]->total_calls);
    }
  }
  return tmp;
}

/* routine to free a merge list built during profile summary */

static void
free_merge_list(struct node ** list, int size)
{
  int         c;

  for (c = 0; c < size; c++)
    free(list[c]);
  free(list);
}

/* ======= Nial primitives ===================== */

/* routine to implement the Nial primitive operation profile, which
  does the summarization of the call tree and outputs the data
  to the file name given in its argument. If the argument is Null
  then the output goes to STDOUT.
*/

void
iprofile()
{
  char        sbuf[1000];
  nialptr     z;
  FILE       *prf = STDOUT;
  int         c1,
              c2;
  int         i;
  double      real_total_time = 0;
  double      profile_duration_time;
  int         NMWIDTH = 30;

  /* grab the argument */
  z = apop();

  if (newprofile) {
    buildfault("no profile available");
    freeup(z);
    return;
  }
  /* handle input argument */
  if (kind(z) == phrasetype) {
    apush(z);
    istring();
    z = apop();
  }
  if (tally(z) != 0) {
    if (!istext(z)) {
      buildfault("profile file name arg is not text");
      freeup(z);
      return;
    }
    else {
      prf = openfile(pfirstchar(z), 'w', 't');
      if (prf == OPENFAILED) {
        buildfault("unable to open specified file to write profile to");
        freeup(z);
        return;
      }
    }
  }
  freeup(z);

  /* If profiling is still on, then turn it off */
  if (profile == true) {
    apush(createbool(0));
    isetprofile();
    apop();
  }

#ifndef OLD_BUILD_SYMBOL_TABLE
  if (symtab) 
    free_symtab();
  symtab = NULL;
  build_symbol_table();
#endif

  profile_duration_time = (calltree->total_time);
  for (i = 0;i <calltree->num_children;i++) {
    real_total_time += calltree->children[i]->total_time;
  }
  /* traverse the call tree placing nodes in the symbol table entries */
  if (!traversed) {
    traverse_tree(calltree);
    traversed = true;
  }
  /* generate the output and place it in the output file or stdout */

  /* if a filename has been specified then write that out */
  if (tally(z)) {
    sprintf(sbuf,"Profile output file: \"%s\"\n\n",pfirstchar(z));
    writechars(prf, sbuf, strlen(sbuf), false);
  }

  sprintf(sbuf, "\nTotal execution time of profile session: \t%f\n", profile_duration_time);
  writechars(prf, sbuf, strlen(sbuf), false);
  sprintf(sbuf, "Total execution time in top level calls:  \t%f\n\n",real_total_time);
  writechars(prf, sbuf, strlen(sbuf), false);

  /* header line for all data */
  sprintf(sbuf, "op name[.tr arg]                 calls[rec]    ");
  writechars(prf, sbuf, strlen(sbuf), false);
  sprintf(sbuf, "time time/call  %% time\n");
  writechars(prf, sbuf, strlen(sbuf), false);

  for (c1 = 0; c1 < symtabsize; c1++) {
    if ((symtab[c1]->num_locs > 0) || (symtab[c1]->num_rcalls > 0)) {
      double      totaloptime = 0;
      int         totalopcalls = 0;
      int         totalropcalls;
      char       *tmp;

      for (c2 = 0; c2 < symtab[c1]->num_locs; c2++) {
        if (symtab[c1]->id != symtab[c1]->locations[c2]->parent->opid)
          /* omit adding calls and time for direct recursions */
        {
          totaloptime += symtab[c1]->locations[c2]->total_time;
          totalopcalls += symtab[c1]->locations[c2]->total_calls;
        }
      }
      totalropcalls = symtab[c1]->num_rcalls;
      sprintf(sbuf, "%s%5d", (tmp = padright(NMWIDTH, (char *) symtab[c1]->name)),
              totalopcalls);
      writechars(prf, sbuf, strlen(sbuf), false);
      free(tmp);
      if (totalropcalls != 0) {
        sprintf(sbuf, "[%5d]", totalropcalls);
        writechars(prf, sbuf, strlen(sbuf), false);
      }
      else {
        sprintf(sbuf, "       ");
        writechars(prf, sbuf, strlen(sbuf), false);
      }
      /* details for each definition */
      sprintf(sbuf, "%8.2f %8.4f %8.1f%s\n",
              totaloptime,
              (totaloptime / totalopcalls),
              100 * (totaloptime / real_total_time),
              ((symtab[c1]->toplevel_call == true)?"<":""));
      writechars(prf, sbuf, strlen(sbuf), false);

      {
        struct node **chlist;
        int         c,
                    used;
        char        tname[40];

        chlist = merge_children(symtab[c1], &used);
        for (c = 0; c < used; c++) {
          char       *tmp;

          if (chlist[c]->opid == symtab[c1]->id)  /* recursions only counted */
            break;
          strcpy(tname, num_to_name(chlist[c]->opid));
          /* details for each definition it calls */
          if (chlist[c]->total_time > 0.0)
            sprintf(sbuf, " %s%5d       %8.2f %8.4f %8.2f\n",
                    (tmp = padright(NMWIDTH - 1, tname)),
                    chlist[c]->total_calls,
                    chlist[c]->total_time,
                    chlist[c]->total_time / chlist[c]->total_calls,
                    100 * (chlist[c]->total_time / totaloptime));
          else
            sprintf(sbuf, " %s%5d       %8.2f %8.4f %8.2f\n",
                    (tmp = padright(NMWIDTH - 1, tname)),
                    chlist[c]->total_calls,
                    chlist[c]->total_time,
                    0.0, 0.0);
          writechars(prf, sbuf, strlen(sbuf), false);
          free(tmp);
        }
        free_merge_list(chlist, used);
      }
      sprintf(sbuf, "\n");
      writechars(prf, sbuf, strlen(sbuf), false);
    }
  }
  if (prf != STDOUT)
    closefile(prf);
  apush(Nullexpr);
  return;
}



/* iprofiletable produces effectively the same results as the
   profile primitive.  This routine returns the result as a
   Nial array.  This allows user manipulation of the profile
   output.
   Each entry in the table(list) has the following form:
 
    [op name,total calls, recursive calls, total time, children]
 
   where children is a table(list) of the same format except the
   children field is Null.
 */


void
iprofiletable()
{
  nialptr     result;        /* result to be returned */
  int         c1,
              c2;
  nialint     num_funs = 0;  /* total number of used functions */
  int         pos = 0;       /* current count of used fucntions */

  if (newprofile) {
    buildfault("no profile available");
    return;
  }

#ifndef OLD_BUILD_SYMBOL_TABLE
  if (symtab) 
    free_symtab();
  symtab = NULL;
  build_symbol_table();
#endif

  /* If profiling is still on, then turn it off */
  if (profile == true) {
    apush(createbool(0));
    isetprofile();
    apop();
  }

  /* traverse the call tree placing nodes in the symbol table entries */
  if (!traversed) {
    traverse_tree(calltree);
    traversed = true;
  }
  /* count the number of called functions so we know how big to   
     make the container array. Funcitons in the symbol table that 
     are not called at all are excluded */

  for (c1 = 0; c1 < symtabsize; c1++)
    if ((symtab[c1]->num_locs > 0) || (symtab[c1]->num_rcalls > 0))
      num_funs++;


  /* create the outer most container to hold the table */

  result = new_create_array(atype, 1, 0, &num_funs);

  for (c1 = 0; c1 < symtabsize; c1++) {
    if ((symtab[c1]->num_locs > 0) || (symtab[c1]->num_rcalls > 0)) {
      double      totaloptime = 0;
      int         totalopcalls = 0;
      int         totalropcalls;
      nialint     len = 5;

      /* create the table entry */
      nialptr     table_entry = new_create_array(atype, 1, 0, &len);

      /* store it in the outer table */
      store_array(result, pos, table_entry);

      for (c2 = 0; c2 < symtab[c1]->num_locs; c2++) {
        if (symtab[c1]->id != symtab[c1]->locations[c2]->parent->opid)
          /* omit adding calls and time for direct recursions */
        {
          totaloptime += symtab[c1]->locations[c2]->total_time;
          totalopcalls += symtab[c1]->locations[c2]->total_calls;
        }
      }
      totalropcalls = symtab[c1]->num_rcalls;

      /* fill in the cells in the table entry */
      store_array(table_entry, 0, makephrase(symtab[c1]->name));
      store_array(table_entry, 1, createint(totalopcalls));
      store_array(table_entry, 2, createint(totalropcalls));
      store_array(table_entry, 3, createreal(totaloptime));
      {
        struct node **chlist;
        nialptr     child_array;
        int         c,
                    used,
                    cpos = 0;
        nialint     children = 0;

        chlist = merge_children(symtab[c1], &used);

        for (c = 0; c < used; c++)
          if (chlist[c]->opid != symtab[c1]->id)  /* recursions only counted */
            children++;

        child_array = new_create_array(atype, 1, 0, &children);
        store_array(table_entry, 4, child_array);

        for (c = 0; c < used; c++) {
          nialptr     child_entry;
          nialint     len = 5;

          if (chlist[c]->opid == symtab[c1]->id)  /* recursions only counted */
            break;

          /* create each child entry and place it in the child list */
          child_entry = new_create_array(atype, 1, 0, &len);
          store_array(child_array, cpos, child_entry);

          /* fill in the information about the child entry */
          store_array(child_entry, 0, makephrase(num_to_name(chlist[c]->opid)));
          store_array(child_entry, 1, createint(chlist[c]->total_calls));
          store_array(child_entry, 2, createint(0));
          store_array(child_entry, 3, createreal(chlist[c]->total_time));
          store_array(child_entry, 4, Null);
          cpos++;
        }
        free_merge_list(chlist, used);
      }
    }
    pos++;
  }
  apush(result);
}






/* The routine tree_to_array is a recursive function that will create a Nial
   array equivalent to the internal C Call graph.
*/

static      nialptr
tree_to_array(struct node * root)
{
  nialptr     newnode;
  nialptr     children;
  nialint     len = 4;


  /* create a 5 cell array to hold the node information */
  newnode = new_create_array(atype, 1, 0, &len);

  /* fill in op name */
  if (root->opid == 0) {
    store_array(newnode, 0, makephrase("TOPLEVEL"));
  }
  /* block req'd */
  else
    store_array(newnode, 0, makephrase(num_to_name(root->opid)));

  /* fill in number of calls */
  store_array(newnode, 1, createint(root->total_calls));

  /* fill in total time */
  store_array(newnode, 2, createreal(root->total_time));

  /* create array for the children and store it in the last cell */
  if (root->num_children > 0) {
    int         i;

    len = root->num_children;
    children = new_create_array(atype, 1, 0, &len);
    store_array(newnode, 3, children);
    /* for each child, call tree_to_array recursively to fill in */
    /* each child cell create above */
    for (i = 0; i < len; i++) {
      store_array(children, i, tree_to_array(root->children[i]));
    }
  }
  else {
    store_array(newnode, 3, Null);
  }
  return (newnode);
}


/* ======= Nial primitives ===================== */

/*
 * routine to implement the Nial primitive operation profiletree,
 * which produces a call graph tree as a Nial array.  This can
 * be parsed manually as the user desires.
 */


void
iprofiletree()
{
  nialptr     result;

  if (newprofile) {
    buildfault("no profile available");
    return;
  }
  result = tree_to_array(calltree);
  apush(result);
}


/* isetprofile is used to turn profiling on or off.
   Since profiling can be switched on or off under program control
   we want to gather several such sessions in one set of
   statistics. The switch newprofile indicates that a new
   set of statistics are desired. It is reset on every use of
   the profile operation.
*/

void
isetprofile()
{
  nialptr     z;
  int         tv,
              oldprofile = profile; /* old value of profiling switch */

  z = apop();
  if (kind(z) == inttype)
    tv = intval(z);
  else if (kind(z) == booltype)
    tv = boolval(z);
  else {
    buildfault("invalid arg to setprofile");
    return;
  }
  if (tv && !oldprofile) {   /* turning on profiling */
    profile = true;
    if (newprofile) {
      inittime();
      calltree = make_node();
      current_node = calltree;
      /* initialize first node */
      set_opid(calltree, 0); /* doesn't correspond to a defn */
      set_start_time(calltree, profile_time());
/*
    nprintf(OF_DEBUG,"start time for call tree%f\n",calltree->start_time);
*/
      swplace = 0;
#ifdef OLD_BUILD_SYMBOL_TABLE
      build_symbol_table();
#endif
      newprofile = false;
    }
  }
  else if (oldprofile != false) {
    double      lasttime = profile_time();

    profile = false;
    if (current_node != calltree) {
      exit_cover1("profiling end does not match begining", NC_WARNING);
    }
    set_end_time(calltree, lasttime);
    add_time(calltree);
  }
  apush(createbool(oldprofile));
}


/* support routines to do the profile timing and store the information */


/* routine called in eval on entry to every definition while
   profiling is on */

void
profile_ops_start(nialptr entr)
{
  struct node *childnode;
  int fc = find_child(current_node, entr);
  
  /*
  fprintf(stderr, "*** Profile ops start\n");
  fprintf(stderr, "entr: %ld\n", entr);
  fprintf(stderr, "current node: %ld\n", current_node);
  fprintf(stderr, "children: %ld\n", current_node->children);
  fflush(stderr);

  fc = find_child(current_node, entr);
  fprintf(stderr, "fc: %ld\n", fc);
  fflush(stderr);
  */
  
  childnode = current_node->children[fc];
  set_start_time(childnode, profile_time());
  current_node = childnode;
}

/* routine called in eval after exit of every definition while
   profiling is on */

/* #pragma argused */
void
profile_ops_stop(nialptr entr)
{
  set_end_time(current_node, profile_time());

  add_time(current_node);

  inc_total_calls(current_node);
  current_node = current_node->parent;
}


void
iclearprofile()
{
  clear_profiler();
  apush(Nullexpr);
}


void
clear_profiler()
{
  if (!newprofile) {
    newprofile = true;
    profile = false;
    traversed = false;
    free_tree(calltree);
    calltree = NULL;
    free_symtab();
    symtab = NULL;
  }
}

#endif             /* PROFILE ) */
