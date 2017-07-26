/*==============================================================

  UTILS.H:  header for UTILS.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the prototypes of various utility functions

================================================================*/

extern nialptr int_to_real(nialptr x);
extern nialptr bool_to_real(nialptr x);
extern nialptr arithconvert(nialptr x, int *newk);
extern nialptr to_real(nialptr x);
extern int  convert(nialptr * x, int *kx, int k);
extern nialptr testfaults(nialptr x, nialptr stdfault);
extern nialptr testbinfaults(nialptr x, nialptr stdfault, int divflag);
extern int  tkncompare(nialptr x, nialptr y);
extern nialptr boolstoints(nialptr x);
extern void cnvtup(char *str);
extern nialint ngetname(nialptr x, char *value);
extern nialptr getuppername(void);
extern char *slower(char *in_s);
