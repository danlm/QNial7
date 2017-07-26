/*==============================================================

  HEADER   INSEL.H

  COPYRIGHT NIAL Systems Limited  1983-2005

  The prototypes of internal routines in insel.c used by other modules.

================================================================*/


extern void select1(nialptr exp); /* used in eval_fun.c */
extern void insert(nialptr exp);  /* used in eval_fun.c */
extern int  choose(nialptr a, nialptr addrs);
   /* used in atops.c insel.c trs.c */
