/*==============================================================

  OPS.H:  header for ATOPS.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the prototypes of the internal array theory operations
  used by other routines.

================================================================*/

extern void append(nialptr x, nialptr y);
   /* used by absmach.c, dll.c, eval.c, parse.c */
extern void hitch(nialptr x, nialptr y);
   /* used by parse.c */
extern void reshape(nialptr x, nialptr y);
   /* used by insel.c, picture.c */
extern void pair(nialptr x, nialptr y);
   /* used by arith.c, dll.c, eval.c, eval_fun.c, picture.c,
      symtab.c, trs.c, userops.c */
extern void splitfb(nialptr z, nialptr * x, nialptr * y);
   /* used by arith.c, compare.c, eval_fun.c */
extern int  simple(nialptr x);
   /* used by compare.c, eval.c, insel.c, linalg.c, picture.c,
      trs.c */
extern nialptr ToAddress(nialint in, nialint * shp, nialint v);
   /* used by trs.c */
extern nialptr generateints(nialint n);
   /* used by insel.c */
extern void findall(nialptr x, nialptr y);
   /* used by trs.c */
extern nialptr nial_raise(nialptr x, int n);
   /* used by picture.c */
extern void cull(nialptr arr, int diversesw);
   /* used by trs.c */
extern int  check_sorted(nialptr x);
   /* trs.c */

