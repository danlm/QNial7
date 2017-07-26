/*==============================================================

  BLDERS.H: header for  MODULE BLDERS.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains prototypes for the builder routines and macros 
  that define parse tree node constructors.

================================================================*/

/* prototypes */

extern nialptr solitaryint(int i0);
extern nialptr mkapair(nialptr i0, nialptr i1);
extern nialptr mkatriple(nialptr i0, nialptr i1, nialptr i2);
extern nialptr mkaquad(nialptr i0, nialptr i1, nialptr i2, nialptr i3);
extern nialptr mkitriple(nialint i0, nialint i1, nialint i2);
extern nialptr mkiquad(nialint i0, nialint i1, nialint i2, nialint i3);
extern nialptr mkiquint(nialint i0, nialint i1, nialint i2, nialint i3, nialint i4);
extern nialptr b_trform(nialptr sym, nialptr env, nialptr opargs, nialptr body);
extern nialptr b_ifexpr(nialint cnt);
extern nialptr b_caseexpr(nialptr ctest, nialptr selectvals, nialptr selectexprs, nialptr exprseqs);
extern nialptr b_blockbody(nialptr locallist, nialptr nonlocallist, nialptr defs, nialptr body);
extern nialptr b_block(nialptr sym, nialptr cenv, int localcnt, nialptr body);
extern nialptr b_opform(nialptr sym, nialptr cenv, int localcnt, nialptr arglist, nialptr body);
extern nialptr b_curried(nialptr op, nialptr argexpr);
extern nialptr b_transform(nialptr tr, nialptr argop);
extern nialptr get_sym(nialptr x);
extern int  tag(nialptr x);
extern nialptr get_name(nialptr x);
extern nialptr get_entry(nialptr x);
extern nialptr b_opcall(nialptr op, nialptr argexpr);
extern nialptr b_constant(int val, nialptr tkn);
extern nialptr combinedaction(nialptr defs, nialptr exprs);


/* builders for parse tree nodes */

#define b_tokenstream(tknlist) mkapair(createint(t_tokenstream),tknlist)

#define b_parsetree(tree) mkapair(createint(t_parsetree),tree)

#define b_array(arry) mkapair(createint(t_array),arr)

#define b_identifier(sym,entr,id) mkaquad(createint(t_identifier),createint((nialint)sym),createint((nialint)entr),id)

#define b_variable(sym,entr) mkitriple(t_variable,sym,entr)

#define b_expression(sym,entr) mkitriple(t_expression,sym,entr)

#define st_idlist() solitaryint(t_idlist)

#define b_indexedassign(id,tr)  mkatriple(createint(t_indexedassign),id,tr)

#define b_pickplace(id,tr)  mkatriple(createint(t_pickplace),id,tr)

#define b_reachput(id,tr)  mkatriple(createint(t_reachput),id,tr)

#define b_slice(id,tr)  mkatriple(createint(t_slice),id,tr)

#define b_choose_placeall(id,tr)  mkatriple(createint(t_choose),id,tr)

#define b_basic(index,role,prop,bincnt) mkiquint(t_basic,index,role,prop,bincnt)

#define st_composition(op) mkapair(createint(t_composition),op)

#define st_list() solitaryint(t_list)

#define st_atlas() solitaryint(t_atlas)

#define st_galaxy() solitaryint(t_galaxy)

#define b_vcurried(op,val) mkatriple(createint(t_vcurried),op,val)

#define st_exprseq(expr) mkapair(createint(t_exprseq),expr)

#define st_defnseq(expr) mkapair(createint(t_defnseq),expr)

#define st_strand(expr) mkapair(createint(t_strand),expr)

#define b_assignexpr(idlist,expr) mkatriple(createint(t_assignexpr),idlist,expr)

#define b_whileexpr(wtest,wes) mkatriple(createint(t_whileexpr),wtest,wes)

#define b_exitexpr(t) mkapair(createint(t_exit),t)

#define b_repeatexpr(res,rtest) mkatriple(createint(t_repeatexpr),res,rtest)

#define b_forexpr(idlist,expr,fexprseq) mkaquad(createint(t_forexpr),idlist,expr,fexprseq)

#define b_definition(idlist,dvalue,fnsw) mkaquad(createint(t_definition),idlist,dvalue,createint((nialint)fnsw))

#define b_ext_declaration(idlist,dvalue) mkatriple(createint(t_ext_declaration),idlist,dvalue)

#define b_commentexpr(comment) mkapair(createint(t_commentexpr),comment)

#define b_nulltree() solitaryint(t_nulltree)

#define b_parendobj(expr) mkapair(createint(t_parendobj),expr)

#define b_dottedobj(expr) mkapair(createint(t_dottedobj),expr)

#define b_scoped_var(idfun, idvar)	mkatriple(createint(t_scoped_var), idfun, idvar)



