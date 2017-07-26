/*==============================================================

  HEADER   TOKEN.H

  COPYRIGHT NIAL Systems Limited  1983-2005

  The macros for the token properties used by scan.c and parse.c

================================================================*/

 /* Token Properties */
#define delimprop      1
#define identprop      2
#define commentprop    3
#define eolprop        4     /* accept in parse.c expects these to */
#define indentprop     5     /* be the last three properties */
#define exdentprop     6

#define constprop     10
#define constphprop   15     /* because sPhrase is 5 */

/* constant tokens are 10 + state value in scan */

#define indentamt      3     /* used to control indenting in descan */
#define commentamt     2     /* used to control comment indenting in descan */

void        initdeparse(void);
