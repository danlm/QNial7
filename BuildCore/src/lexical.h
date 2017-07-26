/*==============================================================

  LEXICAL.H:  

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains macros used by several files

================================================================*/


#define NOERROR          0
#define REALOVERFLOW     1
#define BADREAL          0   /* should be 2 */
#define INTEGEROVERFLOW  3
#define BADINTEGER       0   /* should be 4 */
#define COMPLEXOVERFLOW  5
#define BADCOMPLEX       0   /* should be 6 */

#define COMMENTSYMBOL '%'
#define EMPTYMARKER '\\'
#define ENDOFSCANLINE  '\0'  /* do not change this to a newline. */
#define BLANK      ' '
#define SEMICOLON  ';'
#define MINUS '-'
#define PLUS  '+'
#define DOT '.'
#define GRAVE '`'
#define QUOTE '\''
#define ccdigit(x) ('0'+(x))


#define HASHSYMBOL '#'
#define ESCAPESYMBOL '\\'
#define RIGHTPAREN ')'
#define EXCLAMATION  '!'
#define RIGHTBRACKET  ']'