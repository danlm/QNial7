/*==============================================================

  TRS.H:  header for TRS.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the prototypes of the transformer second order functions

================================================================*/


extern void int_each(void (*f) (void), nialptr x);
extern void real_each(double (*f) (double), nialptr x);
extern void int_eachboth(void (*f) (void), nialptr x, nialptr y);
void sort(nialptr f, nialptr x, int gradesw);
