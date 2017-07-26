/*==============================================================

  GETTERS.H:  

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains macros used in parse.c to get node fields

================================================================*/

/* the next two are for tokenstream and parsetree tags */

#define ptag(x) tag(x)

#define pbody(x) fetch_array(x,1)

#define get_fnsw(x) intval(fetch_array(x,3))

#define get_tr(x) fetch_array(x,1)

#define get_tr2(x) fetch_array(x,2)

#define get_op(x) fetch_array(x,1)

#define get_argexpr(x) fetch_array(x,2)

#define get_argexpr1(x) fetch_array(x,3)

#define get_argop(x) fetch_array(x,2)

#define get_up_tag(x) fetch_array(x,0)

#define get_up_id(x) fetch_array(x,1)

#define get_up_expr(x) fetch_array(x,2)

#define get_ia_left(x) fetch_array(x,1)

#define get_ia_expr(x) fetch_array(x,2)

#define get_c_val(x) fetch_array(x,1)

#define get_c_pv(x) fetch_array(x,2)

#define get_array_val(x) fetch_array(x,1)

#define get_id(x) fetch_array(x,3)

#define get_offset(x) intval(sym_valu(get_entry(x)))

#define get_role(x) sym_role(get_entry(x))

#define get_var_pv(x) sym_name(get_entry(x))

#define get_argval(x) fetch_array(x,2)

#define get_index(x) fetch_int(x,1)

#define get_prop(x) fetch_int(x,3)

#define get_binindex(x) fetch_int(x,4)

#define get_brole(x) fetch_int(x,2)

/* getters for t_opform and t_block */

#define get_trsym(x) fetch_array(x,1)

#define	get_exprsym(x)	fetch_int( x, 1 )

#define get_env(x) fetch_array(x,2)

#define get_cnt(x) intval(fetch_array(x,3))

#define get_arglist(x) fetch_array(x,4)

#define get_body(x) fetch_array(x,5)

#define get_bdy(x) fetch_array(x,4)

/* getters for t_blockbody */

#define get_locallist(x) fetch_array(x,1)

#define get_nonlocallist(x) fetch_array(x,2)

#define get_defs(x) fetch_array(x,3)

#define get_seq(x) fetch_array(x,4)

#define get_opargs(x) fetch_array(x,3)

#define get_trbody(x) fetch_array(x,4)

#define get_test(x,y) fetch_array(x,y)

#define get_conseq(x) fetch_array(x,2)

#define get_altern(x) fetch_array(x,3)

#define get_thenexpr(x,y) fetch_array(x,y+1)

#define get_elseexpr(x,y) fetch_array(x,y)

#define get_idlist(x) fetch_array(x,1)

#define get_expr(x) fetch_array(x,2)

#define get_wtest(x) fetch_array(x,1)

#define get_wexprseq(x) fetch_array(x,2)

#define get_eexprseq(x) fetch_array(x,1)  /* exit expression */

#define get_rexprseq(x) fetch_array(x,1)

#define get_rtest(x) fetch_array(x,2)

#define get_fexprseq(x) fetch_array(x,3)

#define get_ctest(x) fetch_array(x,1)

#define get_svals(x) fetch_array(x,2)

#define get_sexprs(x) fetch_array(x,3)

#define get_eseqs(x) fetch_array(x,4)

#define get_dname(x) fetch_array(x,1)

#define get_dvalue(x) fetch_array(x,2)

#define get_com(x) fetch_array(x,1)

#define get_sps(x) fetch_array(x,3)

#define get_obj(x) fetch_array(x,1)

#define get_idfun(x) fetch_array(x,1)

#define get_idvar(x) fetch_array(x,2)
