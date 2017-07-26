/*==============================================================

  SCAN.H:  header for SCAN.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains macros and prototypes of the scan functions

================================================================*/

void        initscan(void);

#define Upper(x)  (islower(x) ? toupper(x) : x)
#define Lower(x)  (isupper(x) ? tolower(x) : x)
