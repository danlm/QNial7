/*==============================================================

  TRIG.H:  header for TRIG.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the prototypes of the trigonometric functions

================================================================*/

/* constants: define to appropriate precision
   (here are enough digits for any machine)
#define PI 3.141592653589793238462643
#define PIBYTWO 1.570796326794896619231322
*/

#ifndef PI
#define PI 3.1415926535897932
#endif

#define PIBYTWO 1.57079632679489662

#ifdef SCIENTIFIC


            extern void isin(void);
            extern void icos(void);
            extern void itan(void);
            extern void isinh(void);
            extern void icosh(void);
            extern void itanh(void);
            extern void iarcsin(void);
            extern void iarctan(void);
            extern void iarccos(void);
            extern void iexp(void);
            extern void iln(void);
            extern void ilog(void);
            extern void isqrt(void);

#endif
