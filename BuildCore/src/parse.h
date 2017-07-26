/*==============================================================

  PARSE.H:  header for PARSE.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the macros used by the parse and eval functions

================================================================*/

/* ~~~~~~~~~~~~~~~~~~~~ Parser Macros ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#define SUCCEED 1
#define FAIL 0
#define ERROR (-1)


/* ~~~~~~~~~~~~~~~~~~~~~~~~ EVAL case flags ~~~~~~~~~~~~~~~~*/
#define t_constant 1
#define t_variable 2
#define t_basic 3
#define t_opcall 4
#define t_opform 5
#define t_atlas 6
#define t_trform 7
#define t_strand 8
#define t_exprseq 9
#define t_defnseq 10
#define t_definition 11
#define t_ifexpr 12
#define t_assignexpr 13
#define t_whileexpr 14
#define t_repeatexpr 15
#define t_forexpr 16
#define t_caseexpr 17
#define t_block 18
#define t_closure 19
#define t_curried 20
#define t_transform 21
#define t_idlist 22
#define t_identifier 23
#define t_ext_declaration 24
#define t_nulltree 26
#define t_composition 27
#define t_commentexpr 28
#define t_ta_tr 29
#define t_ft_tr 30
#define t_at_tr 31
#define t_trcompose 32
#define t_galaxy 33
#define t_list 34
#define t_vcurried 35
#define t_indexedassign 36
#define t_pickplace 37
#define t_reachput 38
#define t_slice 39
#define t_choose 40
#define t_compiled 41
#define t_array 42
#define t_exit 43
#define t_blockbody 44
#define t_expression 45
#define t_basicexpr 46
#define t_parendobj 47
#define t_dottedobj 48
#define t_basic_binopcall 49
#define t_scoped_var	50
#define t_tokenstream 99
#define t_parsetree 100


extern void parse(int act_only);


/* switch to exclude error numbers when not debugging */

#ifndef DEBUG
#define error(x,y) proderror(x)
#endif

/*  The macros for the parser errors used by parse.c */

#define   exp_expr             1
#define   exp_optn             2
#define   exp_tr               3
#define   exp_rparen           4
#define   exp_semicolon        5
#define   exp_then             6
#define   exp_endif            7
#define   exp_eseq             8
#define   exp_end              9
#define   exp_endwhile        10
#define   exp_do              11
#define   exp_endrepeat       12
#define   exp_until           13
#define   exp_endfor          14
#define   exp_with            15
#define   exp_ident           16
#define   exp_from            17
#define   exp_colon           18
#define   exp_endcase         19
#define   is_defined          20
#define   parse_fail          21
#define   exp_basic           22
#define   exp_def             23
#define   exp_rbracket        24
#define   exp_aot             25
#define   undefined           26
#define   exp_external        27
#define   mismatch_ext        28
#define   exp_body            29
#define   exp_rcurly          30
#define   ill_null_form       31
#define   init_complete       32
#define   ass_nonlocal        33
#define   ref_bfr_ass         34
#define   invalid_assign      35
#define   exp_quotepgm        36
#define   exp_primary         38
#define   exp_stmtexpr        39
#define   exit_context        40
#define   inv_external        41
#define   parser_error        42
#define   incomplete_parse    43
#define   role_error          45
#define   exp_defn            46
#define   local_reused        47
#define   arg_reused          48
#define   undefined1          49
#define   invalid_optn        50
#define   exp_var             51
#define   sysname_scope       52
#define   no_scope            53

