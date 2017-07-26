/*==============================================================

  ARITH.H:  header for ARITH.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  This contains the prototypes of the arithmetic and other functions

================================================================*/


/* sumints is used in atops.c */
extern int  sumints(nialint * ptrx, nialint n, nialint * res);

/* prodints is used in atops.c and trs.c */
extern int  prodints(nialint * ptrx, nialint n, nialint * res);
