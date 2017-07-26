/*==============================================================

  LOGICOPS.H:  HEADER FOR LOGICOPS.C

  COPYRIGHT NIAL Systems Limited  1983-2005

   This contains the prototypes of the logical functions used
   by other modules.

================================================================*/

extern void b_or(void); /* used by compare.c and logicops.c */
extern void b_and(void); /* used by compare.c and logicops.c */

extern int  orbools(nialptr x, nialint n);
              /* used by compare.c and logicops.c */

extern int  andbools(nialptr x, nialint n);
              /* used by compare.c and logicops.c */

extern int  xorbools(nialptr x, nialint n);

