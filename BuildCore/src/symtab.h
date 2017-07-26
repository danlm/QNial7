/*==============================================================

  SYMTAB.H:  header for SYMTAB.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains macros and the prototypes of the symbol table functions

================================================================*/

 /* Symbol Table Definitions  - Btree stored as a Nial array.     */


#define sym_entrysize 6

#define sym_name(entry)   fetch_array(entry,0)
#define sym_rf(entry)     fetch_array(entry,1)
#define sym_valu(entry)   fetch_array(entry,2)
#define sym_left(entry)   fetch_array(entry,3)
#define sym_rght(entry)   fetch_array(entry,4)
#define sym_flag(entry)   intval(fetch_array(entry,5))
#define sym_role(entry)   (intval(sym_rf(entry))& 0x000F)
#define sym_trflg(entry)  ((intval(sym_rf(entry))>>4) & 0x0001)
#define sym_brflg(entry)  ((intval(sym_rf(entry))>>5) & 0x0001)

#define st_s_name(entry,nm)     replace_array(entry,0,nm)
#define st_s_rf(entry,rf)   replace_array(entry,1,rf)
#define st_s_valu(entry,val)     replace_array(entry,2,val)
#define st_s_left(entry,l)   replace_array(entry,3,l)
#define st_s_rght(entry,r)   replace_array(entry,4,r)
#define st_s_flag(entry,f)  replace_array(entry,5,createint(f))
#define st_s_role(entry,role)   replace_array(entry,1,createint(role))

#define st_s_trflg(entry,f) st_s_rf(entry,\
			createint((f<<4)|(sym_brflg(entry)<<5) \
			|sym_role(entry)))
#define st_s_brflg(entry,f) st_s_rf(entry,\
			createint((f<<5)|(sym_trflg(entry)<<4) \
			|sym_role(entry)))

#define get_root(sym)  fetch_array(sym,0)
#define get_symtabname(sym) fetch_array(sym,3)
#define get_spval(sym) intval(fetch_array(sym,1))
#define get_sp(sym)    fetch_array(sym,1)
#define symprop(sym)   intval(fetch_array(sym,2))
#define set_symtabname(sym,name)   replace_array(sym,3,name)
#define replace_symprop(sym,prop)  replace_array(sym,2,createint(prop))

#define notfound       (nialptr)(-2)  /* Used in 'blookup'. */

/* symbol table record defines */

#define store_root(sym,val) replace_array(sym,0,val)
#define store_sp(sym,sp) replace_array(sym,1,sp)
#define st_symprop(sym,prop) replace_array(sym,2,createint((nialint)prop))

/* symbol table properties */

#define stpglobal    1
#define stpopen      2
#define stpclosed    3
#define stpparameter 4

/* search types */

#define active    1
#define passive   2
#define statics   3
#define dynamic   4
#define formal    5
#define nonlocal  6
#define globals   7


/* symtab.c */

extern nialptr addsymtab(int prop, char * stname); 
      /* called in parse.c  */
extern nialptr mkSymtabEntry(nialptr sym, nialptr name, nialptr role, 
        nialptr value, int sysflag); /* called in lib_main.c parse.c */
extern nialptr lookup(nialptr name, nialptr * sym, int searchtype);
   /* called in eval.c lib_main.c mainlp.c parse.c symtab.c */
extern nialptr Blookup(nialptr entr, nialptr name);
   /* called in eval.c eval_fun.c scan.c symtab.c */
