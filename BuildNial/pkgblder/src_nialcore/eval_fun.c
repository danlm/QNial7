/*==============================================================

  FILE EVAL_FUN.C   used to produce two versions of eval() when
       included in eval.c

  COPYRIGHT NIAL Systems Limited  1983-2016


================================================================*/

/* This file defines the internal routine eval that is used for recursion
   and also called from insel and mainlp.c. The file is included twice
   in eval.c with EVAL_DEBUG defined and not defined. 
 
   The files needed for this code are brought in by eval.c
 */

/* see comments in eval.c for the role played by eval() */

void
#ifdef EVAL_DEBUG
d_eval(nialptr exp)
#else
n_eval(nialptr exp)
#endif
{
  nialptr     oldexp = saveexp; /* keep for use by showexpr */
  saveexp = exp;
  changetrigger = false;
#ifdef EVAL_DEBUG
  usercallchanged = false;   /* initialize call  flags */
  primcallchanged = false;
#endif

  /* if a signal has been indicated go to top level */
#ifdef USER_BREAK_FLAG
  checksignal(NC_CS_NORMAL);
#endif

  /* code to test if stack has overflowed */
  if (CSTACKFULL)
    longjmp(error_env, NC_WARNING);

recur:
  if (exp == Nullexpr || kind(exp) == faulttype) 
  { /* result is argument */
    apush(exp);
  }
  else {
    switch (tag(exp)) {
      case t_nulltree:       /* place holder in case statements and atlases */
          apush(Nullexpr);
          break;

      case t_parsetree:      /* a "cast", i.e. an array denoting a piece of
                              *code. Result is the parse tree. */
          apush(exp);
          break;

      case t_constant:       /* a code tree denoting a constant which has
                              * been evaluated by the parser. */
          {
            nialptr     v = get_c_val(exp);

            if (triggered && kind(v) == faulttype) {
              if (v != Nullexpr && v != Eoffault && v != Zenith && v != Nadir)
                v = makefault(pfirstchar(v)); /* force a triggering */
              apush(v);
            }
            else
              apush(v);
          }
          break;

      case t_variable:       /* A code tree denoting a variable that has an
                              * array value. Returns the variable value on
                              * stack. */
          apush(fetch_var(get_sym(exp), get_entry(exp)));
          break;

      case t_basic_binopcall:/* infix call of a basic binary operation */
          {
            nialptr     arg = Null,
                        fn = get_op(exp);
            int         argflag = false;

            /* evaluate the left and then the right argument on the stack */
#ifdef EVAL_DEBUG
            d_eval(get_argexpr(exp));
            d_eval(get_argexpr1(exp));
#else
            n_eval(get_argexpr(exp));
            n_eval(get_argexpr1(exp));
#endif
            if (kind(top) == phrasetype || kind(top) == faulttype) {
              if (top == topm1) 
              { /* need to protect arg when items are the same
                   since the apply will attempt to free both */
                argflag = true;
                arg = top;
                incrrefcnt(top);
              }
            }
            /* apply the basic binary operation here to avoid expense
               of apply call */
            APPLYBINARYPRIM(fn);
#ifdef FP_EXCEPTION_FLAG
            fp_checksignal();
#endif
            if (argflag) {
              decrrefcnt(arg); /* remove protection and do freeup now */
              freeup(arg);
            }
          }
          break;

      case t_opcall:         /* Prefix call of an operation */
          /* evaluate the argument and then apply the op. However if the op
             is curried it is necessary to evaluate the curried arg first to
             get expected semantics. */
          {
            nialptr     op = get_op(exp);

#ifdef EVAL_DEBUG

            /* hold a copy of d_onestep */
            int         local_onestep = d_onestep;
            int         local_inloop = d_inloop;

            d_inloop = false;
            /* if we want to skip operation calls or we have a system defined
             * operation, then we turn onestep off and skip over it. */
            debug_flags("into opcall\n");
            if (d_next || is_systemop(op))
              d_onestep = false;
#endif

            if (tag(op) == t_basic) { /* do the apply here to avoid expense
                                       of apply call */
#ifdef EVAL_DEBUG
              d_eval(get_argexpr(exp)); /* evaluate the opcall argument */
#else
              n_eval(get_argexpr(exp)); /* evaluate the opcall argument */
#endif
              APPLYPRIMITIVE(op);
#ifdef FP_EXCEPTION_FLAG
              fp_checksignal();
#endif
#ifdef EVAL_DEBUG
              /* restore the onestep variable if we turned it off earlier */
              if (d_next || is_systemop(op))
                d_onestep = local_onestep;
              d_inloop = local_inloop;
#endif
              break;
            }
            else
            if (tag(op) == t_curried) {
              nialptr     leftval,
                          rightval;
              int         leftflag,
                          rightflag;

#ifdef EVAL_DEBUG
              d_eval(get_argexpr(op));  /* evaluate the curried argument */
#else
              n_eval(get_argexpr(op));  /* evaluate the curried argument */
#endif
              leftflag = kind(top) >= phrasetype;
              if (leftflag)
                apush(top);  /* to protect the argument in weird cases. This
                              is necessary because a phrase or fault may be
                              shared without its ref count being set. This
                              was the source of a very subtle error. */
              leftval = apop();
#ifdef EVAL_DEBUG
              d_eval(get_argexpr(exp)); /* evaluate the opcall argument */
#else
              n_eval(get_argexpr(exp)); /* evaluate the opcall argument */
#endif
              rightflag = kind(top) >= phrasetype;
              if (rightflag)
                apush(top);  /* to protect the argument in weird cases. */
              rightval = apop();
              pair(leftval, rightval);
              apply(get_op(op));  /* apply the op within the curried optn */
              if (rightflag) {
                swap();
                freeup(apop()); /* free right arg val if it is temporary */
              }
              if (leftflag) {
                swap();
                freeup(apop()); /* free left arg if it is temporary */
              }
            }
            else {
#ifdef EVAL_DEBUG
              d_eval(get_argexpr(exp)); /* evaluate the opcall argument */
#else
              n_eval(get_argexpr(exp)); /* evaluate the opcall argument */
#endif
              apply(op);     /* apply the opcall optn */
            }
#ifdef EVAL_DEBUG
            /* restore the onestep variable as we leave here if we saved it */
            if (d_next || is_systemop(op))
              d_onestep = local_onestep;
            d_inloop = local_inloop;
#endif
          }
          break;

      case t_basic:          /* evaluate a basic expression */
          {
            APPLYPRIMITIVE(exp);
#ifdef FP_EXCEPTION_FLAG
            fp_checksignal();
#endif
          }
          break;

      case t_list:     /* code tree is an atlas, with 0 or more expressions */
          if (tally(exp) == 1) { /* no items in atlas */
            apush(Null);
            break;
          }
          /* else fall through to strand */

      case t_strand:         /* a list of 1 or more expressions that
                                evaluates to a list of their values. They are
                                evaluated left to right */
          {
            nialint     i,
                        tallyexp;

            tallyexp = tally(exp);
            for (i = 1; i < tallyexp; i++)
#ifdef EVAL_DEBUG
              d_eval(fetch_array(exp, i));
#else
              n_eval(fetch_array(exp, i));
#endif
            mklist(tally(exp) - 1);
          }
          break;

      case t_defnseq:        /* definition sequence is evaluated as if it
                                were an expression sequence. */
      case t_exprseq:        /* expression sequence: eval each expression in
                                left to right order, unless interrupted by an
                                exit. */
          {
            nialint     i,
                        tv;

#ifdef EVAL_DEBUG
            int         local_toend = d_toend;  /* local copy of toend */
#endif
#ifndef EVAL_DEBUG
            /* This peels away the extra t_exprseq wrapper that is
               always placed around 1 element expression sequences. */
            if (!trace && tally(exp) == 2) {
              exp = fetch_array(exp, 1);
              goto recur;
            }
#endif
            apush(Nullexpr); /* to be thrown away as the loop starts */
            i = 1;
            tv = tally(exp);
            debug_flags("before loop in eseq");
            while ((i < tv) && !nialexitflag) {
#ifdef EVAL_DEBUG
              int         local_onestep = d_onestep;  /* for keeping a copy of
                                                       * d_onestep */

              int         isoutput = false; /* indicates if text will be
                                             * printed */

#endif
              freeup(apop());
#ifdef EVAL_DEBUG
              /* This is where we output the nial expression that we are
                 about to execute. We only display this if we are stepping
                 (d_onestep) and we are NOT resuming. If we want a single 
                 step and we have not requested a force resumethen 
                execute the expression  */
              if (d_onestep) {

                /* A hack to prevent the odd line being printed if we are 
                   iterating through a couple of TOENDn commands.  
                   When TOENDN would take the user out of 2 or more loops, 
                   then this will prevent the first line of
                   the parent loop from being printed  */
                if ((tag(exp) != t_commentexpr) && (tag(exp) != t_defnseq))
                  if (!((d_repeatcount > 0) && (d_lastcommand == TOEND_LOC)))
                    /* isoutput indicates if anything was printed */
                    isoutput = !showexpr(fetch_array(exp, i),
                                  ((d_repeatcount > 0) ? TRACE : QUESTION));
              }

              /* Here we go back into the break mode to get the next
                 debugging command only if 1) we are still stepping and 2)
                 there was text printed. */
              if (isoutput && d_onestep) {
                break_loop(STEPf);
              }

              
              debug_flags("before\n");
#endif
              /* do the eval */
#ifdef EVAL_DEBUG
              d_eval(fetch_array(exp, i));
#else
              n_eval(fetch_array(exp, i));
#endif
#ifdef EVAL_DEBUG
              debug_flags("after\n");

              /* We have just done an eval.  Here is where we "may" output
                 the results of the just evaluated expression.
                 One of the following cases may just have happened: - the eval()
                 call executed a "break" command or the break_loop() call may
                 have changed the current debugging mode.
                WE OUTPUT the RESULT ONLY IF: 
                  - If d_stepin  is set then we don't want to picture the 
                    output here, but we will let the trace code do it at the end 
                    of the eval call. 
                  - We are not resuming. 
                  - We are not continuing to the end "d_toend". 
                  - If d_onestep was on before this expression was executed. 
                    (if it became true, then that could have happened during the
                    break_loop() call above OR somewhere within the eval()
                    call.) 
                    If d_onestep became true during this t_exprseq then
                    we don't want to output the result of the evaluated
                    expression because we haven't outputed the expression that
                    produced it! 
                  - d_quiet is true, (we want to suppres output)
               */
              if (!d_toend && !d_forceresume &&
                  !d_stepin && !d_quiet &&
                  local_onestep && top != Nullexpr) {
                int         tg = tag(exp);

                /* The following expressions do not produce interesting output */
                if (tg != t_constant && tg != t_block
                    && tg != t_ifexpr && tg != t_caseexpr && tg != t_forexpr
                    && tg != t_repeatexpr && tg != t_whileexpr
                    && tg != t_commentexpr) {
                  /* print out value of expression  */
                  apush(top);/* duplicates result for use by ipicture */
                  ipicture();
                  show(apop());
                }
              }
              /* */
#endif
              i++;
            }                /* end while */

#ifdef EVAL_DEBUG
            /* If we have finished the expression sequence, and toend
               was false, and has become true, then turn onestep back on so
               we can continue stepping */
            if (!local_toend && d_toend && !d_inloop) {
              debug_flags("before change at end of exprseq");
              d_toend = false;
              d_onestep = true;
            }
#endif
            debug_flags("end of exprseq");
          }
          break;

      case t_exit:           /* exit from one level of for loop, while loop
                              * or repeat loop */
          nialexitflag = true;  /* set flag to interrupt the exprseq loop */
          exp = get_eexprseq(exp);
          if (triggered) {   /* do a recursive call to get correct jmp_buf */
#ifdef EVAL_DEBUG
            d_eval(exp);
#else
            n_eval(exp);
#endif
            break;
          }
          goto recur;

      case t_assignexpr:     /* assignment expression. The left hand side must be
                                a variable or list of them since it has been 
                                checked by the parser */
          {
            nialptr     idlist;

            idlist = get_idlist(exp);
            /* evaluate right hand side */
#ifdef EVAL_DEBUG
            d_eval(get_expr(exp));  
#else
            n_eval(get_expr(exp)); 
#endif
            /* do the assignment and check for success */
            if (!assign(idlist, top, false, true)) {
              freeup(apop());
              apush(makefault("?assignment"));
            }
          }
          break;

      case t_scoped_var:     /* reference to a variable in a calling scope.
                                This allows a dynamic lookup of a variable
                                in an explicit scope */
          {
            nialptr     var_id,
                        fun_tree = get_idfun(exp),
                        fun_id = get_entry(fun_tree),
                        varphrase = get_name(get_idvar(exp));
            nialint     SP;
            char        errmsg[200];

            nialptr     fun_sym = get_trsym(sym_valu(fun_id));

            /* grab the stack pointer */
            SP = get_spval(fun_sym);

            /* if the scope is not active then abort */
            if (SP == -1) {
              sprintf(errmsg, " %s is not active! ", pfirstchar(sym_name(fun_id)));
              buildfault(errmsg);
            }
            else {           /* lookup variable name in specified scope */
              var_id = Blookup(get_root(fun_sym), varphrase);

              /* if it is found and a variable then continue */
              if (var_id != notfound) {
                if (sym_role(var_id) == Rvar) { /* push it */
                  apush(stkarea[SP + (intval(sym_valu(var_id)))]);
                }
                else {
                  sprintf(errmsg, " %s not a variable!", pfirstchar(varphrase));
                  buildfault(errmsg);
                }
              }
              else {
                sprintf(errmsg, " %s is not accessible", pfirstchar(varphrase));
                buildfault(errmsg);
              }
            }
            break;
          }

      case t_ifexpr:         /* if-then-else expression with elseif clauses.  */
          {
            nialptr     val;
            nialint     noexprs,
                        i;
            int         itedone;

            noexprs = tally(exp);
            i = 1;
            itedone = false;
            while ((noexprs - i) > 1) { /* has test and then expr */
#ifdef EVAL_DEBUG
              d_eval(get_test(exp, i)); /* evaluate the ith test */
#else
              n_eval(get_test(exp, i)); /* evaluate the ith test */
#endif
              val = apop();
              if (kind(val) == booltype && valence(val) == 0) {
                if (boolval(val)) { /* test is true */
                  itedone = true;
                  freeup(val);
#ifdef EVAL_DEBUG
                  d_eval(get_thenexpr(exp, i)); /* evaluate i+1 st expression */
#else
                  n_eval(get_thenexpr(exp, i)); /* evaluate i+1 st expression */
#endif
                  break;  /* leave loop early */
                }
                else
                  i = i + 2;
              }
              else {
                freeup(val);
                itedone = true;
                apush(Logical); /* answer is ?L if a test is non-boolean */
                break;
              }
              freeup(val);
            }
            if (!itedone) /* all test expressions are False */
            { if ((noexprs - i) == 1) /* there is an else expression */
#   ifdef EVAL_DEBUG
                d_eval(get_elseexpr(exp, i));
#else
                n_eval(get_elseexpr(exp, i));
#endif
              else
                { apush(Nullexpr); }
            }
          }
          break;

      case t_whileexpr:      /* While expression.  */
          {
            nialptr     test,
                        tval,
                        body;

#ifdef EVAL_DEBUG
            int         localinloop = d_inloop;
            int         local_toend = d_toend;

#endif
            int         flag; /* signals valid logical value in test */

            nialexitflag = false;
            test = get_wtest(exp);  /* test expression */
            body = get_wexprseq(exp); /* body of the loop */

            apush(Nullexpr);  /* default result body not executed */
#ifdef EVAL_DEBUG
            d_eval(test);
#else
            n_eval(test);
#endif
            tval = apop();
            flag = kind(tval) == booltype && valence(tval) == 0;
#ifdef EVAL_DEBUG
            d_inloop = true;
#endif

            while (flag && boolval(tval)) {
              freeup(apop());/* previous loop value */
              freeup(tval);  /* free up test value */
#ifdef EVAL_DEBUG
              d_eval(body);
#else
              n_eval(body);
#endif
              if (nialexitflag) {
#ifdef EVAL_DEBUG
                d_inloop = localinloop;
                /* If we entered the loop with d_toend == false, and it is
                   now true, then the user entered the debug command "toend"
                   sometime during the loop.  But at this point, we are no
                   longer in the loop, and we can restore the d_toend flag
                   (and d_onestep). */

                /* toend turned on during this function */
                if (!local_toend && d_toend) {
                  d_toend = false;
                  d_onestep = true;
                }
#endif
                break;       /* leave loop since exit expression was done */
              }
#ifdef EVAL_DEBUG
              /* re-evaluate the test and loop */
              d_eval(test);
#else
              n_eval(test);
#endif
              tval = apop();
              flag = kind(tval) == booltype && valence(tval) == 0;
            }
            if (!flag && !nialexitflag) { /* not a boolean value in test */
              freeup(tval);
              freeup(apop());
              apush(Logical);
#ifdef EVAL_DEBUG
              d_inloop = localinloop;
              /* If we entered the loop with d_toend == false, and it is now
                 true, then the user entered the debug command "toend"
                 sometime during the loop.  But at this point, we are no
                 longer in the loop, and we can restore the d_toend flag (and
                 d_onestep). */

              /* toend turned on during this function */
              if (!local_toend && d_toend) {
                d_toend = false;
                d_onestep = true;
              }
#endif
              break;
            }
            if (nialexitflag)
              nialexitflag = false; /* signal used for one level */
            else
              freeup(tval);  /* free last test value in loop */

#ifdef EVAL_DEBUG
            d_inloop = localinloop;
            /* If we entered the loop with d_toend == false, and it is now
             * true, then the user entered the debug command "toend" sometime
             * during the loop.  But at this point, we are no longer in the
             * loop, and we can restore the d_toend flag (and d_onestep). */

            /* toend turned on during this function */
            if (!local_toend && d_toend) {
              d_toend = false;
              d_onestep = true;
            }
#endif
          }
          break;

      case t_repeatexpr:     /* Repeat expression */
          {
            nialptr     test,
                        body,
                        tval;

#ifdef EVAL_DEBUG
            int         localinloop = d_inloop;
            int         local_toend = d_toend;

#endif
            int         tv;

            nialexitflag = false;
            test = get_rtest(exp);
            body = get_rexprseq(exp);
            apush(Nullexpr);

#ifdef EVAL_DEBUG
            d_inloop = true;
#endif

            do {
              freeup(apop());/* free up previous loop value */
              /* evaluate the body */
#ifdef EVAL_DEBUG
              d_eval(body);
#else
              n_eval(body);
#endif
              if (!nialexitflag) {
                /* evaluate the test */
#ifdef EVAL_DEBUG
                d_eval(test);
#else
                n_eval(test);
#endif
                tval = apop();
                if (kind(tval) != booltype || valence(tval) != 0) {
                  freeup(tval);
                  freeup(apop());
                  apush(Logical); /* nonlogical value in test */
#ifdef EVAL_DEBUG
                  d_inloop = localinloop;
                  /* If we entered the loop with d_toend == false, and it is
                     now true, then the user entered the debug command
                     "toend" sometime during the loop.  But at this point, we
                     are no longer in the loop, and we can restore the
                     d_toend flag (and d_onestep). */

                  /* toend turned on during this function */
                  if (!local_toend && d_toend) {
                    d_toend = false;
                    d_onestep = true;
                  }
#endif

                  break;
                }
                tv = boolval(tval);
                freeup(tval);
              }
              else {
#ifdef EVAL_DEBUG
                d_inloop = localinloop;
                /* If we entered the loop with d_toend == false, and it is
                   now true, then the user entered the debug command "toend"
                   sometime during the loop.  But at this point, we are no
                   longer in the loop, and we can restore the d_toend flag
                   (and d_onestep). */

                /* toend turned on during this function */
                if (!local_toend && d_toend) {
                  d_toend = false;
                  d_onestep = true;
                }
#endif
                break;
              }
            }
            while (!tv);
            nialexitflag = false; /* signal used for one level */
#ifdef EVAL_DEBUG
            d_inloop = localinloop;
            /* If we entered the loop with d_toend == false, and it is now
               true, then the user entered the debug command "toend" sometime
               during the loop.  But at this point, we are no longer in the
               loop, and we can restore the d_toend flag (and d_onestep). */

            /* toend turned on during this function */
            if (!local_toend && d_toend) {
              d_toend = false;
              d_onestep = true;
            }
#endif
          }
          break;

      case t_forexpr:        /* For expression */
          {
            nialptr     idlist,
                        body,
                        ival;

#ifdef EVAL_DEBUG
            int         localinloop = d_inloop;
            int         local_toend = d_toend;

#endif
            nialint     cnt,
                        i;

            nialexitflag = false;
            idlist = get_idlist(exp);
            body = get_fexprseq(exp);
            /* eval the with expression */
#ifdef EVAL_DEBUG
            d_eval(get_expr(exp));  
#else
            n_eval(get_expr(exp));  
#endif
            ival = apop();
            cnt = tally(ival);
            apush(Nullexpr);
#ifdef EVAL_DEBUG
            d_inloop = true;
#endif
            for (i = 0; i < cnt; i++) {
              /* free last value and assign variable */
              freeup(apop());
              assign(idlist, fetchasarray(ival, i), false, false);
#ifdef EVAL_DEBUG
              d_eval(body);
#else
              n_eval(body);
#endif
              if (nialexitflag) {
#ifdef EVAL_DEBUG
                d_inloop = localinloop;
                /* If we entered the loop with d_toend == false, and it is
                   now true, then the user entered the debug command "toend"
                   sometime during the loop.  But at this point, we are no
                   longer in the loop, and we can restore the d_toend flag
                   (and d_onestep). */

                /* toend turned on during this function */
                if (!local_toend && d_toend) {
                  d_toend = false;
                  d_onestep = true;
                }
#endif
                break;
              }
            }
            nialexitflag = false; /* signal used for one level */
            freeup(ival);
#ifdef EVAL_DEBUG
            d_inloop = localinloop;
            /* If we entered the loop with d_toend == false, and it is now
               true, then the user entered the debug command "toend" sometime
               during the loop.  But at this point, we are no longer in the
               loop, and we can restore the d_toend flag (and d_onestep). */

            /* toend turned on during this function */
            if (!local_toend && d_toend) {
              d_toend = false;
              d_onestep = true;
            }
#endif
          }
          break;


      case t_caseexpr:       /* Case  expression */
          {
            nialptr     ca,
                        val,
                        body,
                        exprseqs;
            nialint     in = 0;
            int         found = false;
            /* eval the case test expresssion */
#ifdef EVAL_DEBUG
            d_eval(get_ctest(exp));
#else
            n_eval(get_ctest(exp));
#endif
            val = top;       /* leave it on stack to protect it */
            ca = get_svals(exp);
            /* find constant that corresponds to expr value    */
            /* if none, in = tally(ca) selects else case in expr_seq */
            while (!found && in < tally(ca))
              found = equal(fetchasarray(ca, in++), val);
            freeup(apop());  /* remove val */
            exprseqs = get_eseqs(exp);
            if (found)
              --in;          /* move in back to found case */
            body = fetch_array(exprseqs, in); /* pick up found case or else
                                                expression (Nulltree if none) */
#ifdef EVAL_DEBUG
            d_eval(body);
#else
            n_eval(body);
#endif
          }
          break;

      case t_ext_declaration:/* all the work has been done by parse */
      case t_commentexpr:    /* a comment has no value */
          apush(Nullexpr);
          break;

      case t_definition:     /* Definition. place in value cell using assign. 
                                A Defn has no value */
          {
            nialptr     idlist,
                        expr;

            idlist = get_idlist(exp);
            expr = get_dvalue(exp);
            assign(idlist, expr, (int) get_fnsw(exp), false);
            apush(Nullexpr);
          }
          break;

      case t_block:          /* a block is an exprseq with its own
                                environment */
          {
            nialptr     save_env,
                        blockbdy = get_bdy(exp),
                        defs = get_defs(blockbdy);
            nialint     nvars = get_cnt(exp);

            nonlocs = get_nonlocallist(blockbdy); /* needed by lookup */
            /* set up new environment */
            prologue(get_env(exp), nvars, &save_env); 
            if (defs != grounded) {
#ifdef EVAL_DEBUG
              d_eval(defs);
#else
              n_eval(defs);
#endif
              apop();        /* remove Nullexpr from stack */
            }
#ifdef EVAL_DEBUG
            d_eval(get_seq(blockbdy));
#else
            n_eval(get_seq(blockbdy));
#endif
            epilogue(save_env, nvars);  /* restore environment */
          }
          break;

          /* insert and select notations are handled by select1
             in module insel.c */
      case t_pickplace:
      case t_reachput:
      case t_slice:
      case t_choose:
          select1(exp);
          break;

      case t_indexedassign:
          /* indexed assignment notations are handled by inset
             in module insel.c */

          insert(exp);
          break;

      case t_expression:     /* look up a named expression & eval it */
          {
            nialptr     expr,
                        entr = get_entry(exp),
                        sym;
            int         local_trace = trace;
            int         namestacked = false;

#ifdef PROFILE
            int         pnamestacked = false;

#endif
#ifdef EVAL_DEBUG
            /* Keep a local copy of onestep, and turn off stepping if the
               user has requested a "next" or if this is a system expression */
            int         local_onestep = d_onestep;
            int         local_inloop = d_inloop;

            d_inloop = false;
            if (d_next || sym_flag(entr))
              d_onestep = false;
#endif

            sym = get_sym(exp);
            expr = fetch_var(sym, entr);
            /* protect expr in case of redefinition , e.g. retry is { execute
             * 'retry is 5'; } */
            apush(expr);
            if (sym_trflg(entr)) {  /* flag is on for tracing the expression */
              nprintf(OF_NORMAL_LOG, "...trace evaluation of expression");
              showexpr(exp, TRACE);
              trace = true;
            }
            else
              trace = false;

            if (sym_brflg(entr)) {  /* flag is on for breaking in this
                                       expression */
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
              /* add expression to call stack */
              enter_op(entr);
              namestacked = true;
            }
#ifdef EVAL_DEBUG
            d_eval(expr);
#else
            n_eval(expr);
#endif
            trace = local_trace;
#ifdef PROFILE
            if (pnamestacked)
              profile_ops_stop(entr);
#endif
            if (namestacked)
             /* remove expression from call stack */
              exit_op();
            /* remove saved expression */
            swap();
            freeup(apop());
#ifdef EVAL_DEBUG
            if (d_next || sym_flag(entr)) {
              /* This checks to see if onestep was turned on here */
              if (!(!local_onestep && d_onestep))
                d_onestep = local_onestep;
            }
            d_inloop = local_inloop;
#endif
            break;
          }

      case t_parendobj:
      case t_dottedobj:
          /* evaluate the expression, avoiding call when no triggering */
          exp = get_obj(exp);
          if (triggered) {   /* do a recursive call to get correct jmp_buf */
#ifdef EVAL_DEBUG
            int         save_onestep = d_onestep;

            d_onestep = false;  /* turn off so that debugging does not stop
                                   in parened expressions */
            d_eval(exp);
            d_onestep = save_onestep;
#else
            n_eval(exp);
#endif
            break;
          }
          goto recur;

      default:
#ifdef DEBUG
          nprintf(OF_DEBUG, "invalid case in eval\n");
          nprintf(OF_DEBUG, "tag is %d\n", tag(exp));
          nabort(NC_ABORT);
#else
          apush(makefault("?invalid case in eval"));
#endif

    }


#ifdef EVAL_DEBUG

    /* trace is handled after case in eval */

    if ((d_stepin || trace) && top != Nullexpr) {
      int         tg = tag(exp);

      if (tg != t_constant && tg != t_exprseq && tg != t_block
          && tg != t_ifexpr && tg != t_caseexpr && tg != t_forexpr
        && tg != t_repeatexpr && tg != t_whileexpr && tg != t_commentexpr) {
        showexpr(exp, TRACE);
        /* print out value of expression  */
        if (!d_quiet) {
          apush(top);        /* duplicates result for use by ipicture */
          ipicture();
          show(apop());
        }
      }
    }
#else
    if (trace && top != Nullexpr) {
      int         tg = tag(exp);

      if (tg != t_constant && tg != t_exprseq && tg != t_block
          && tg != t_ifexpr && tg != t_caseexpr && tg != t_forexpr
        && tg != t_repeatexpr && tg != t_whileexpr && tg != t_commentexpr) {
        showexpr(exp, TRACE);
        /* print out value of expression  */
        if (!d_quiet) {
          apush(top);        /* duplicates result for use by ipicture */
          ipicture();
          show(apop());
        }
      }
    }
#endif

#ifdef DEBUG
    if (top <= 0 || top >= memsize) {
      nprintf(OF_DEBUG, "invalid result of eval tag %d\n", tag(exp));
      nabort(NC_ABORT);
    }
#endif
  }
  saveexp = oldexp;
  if (changetrigger)
    triggered = triggervalue;
#ifdef EVAL_DEBUG
  if (usercallchanged)
    userroutines = usercallvalue;
  if (primcallchanged)
    primroutines = primcallvalue;
#endif
}
