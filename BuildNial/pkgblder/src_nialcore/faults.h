/*==============================================================

  FAULTS.H:  

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains Macros to generate common faults

================================================================*/

#define Cipher makefault("?")
#define Arith makefault("?A")
/* need Logical not to produce a temporary 
#define Logical makefault("?L")
*/
#define Divzero makefault("?div")
