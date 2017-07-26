/*==============================================================

  HEADER   STATES.H

  COPYRIGHT NIAL Systems Limited  1983-2005

  The macros for the states within the scanner

================================================================*/

/*    Below are the tables required by the scanner module to provide
class and transition maps.  The names of states and character classes are also
defined.
*/

#ifndef LEX

#define MINerror       40
#define MAXerror       48

 /* Tokenizer states. */
#define sStart          0
#define sIdentifier     1
#define sComment        2
#define sString         3
#define sString1        4
#define sPhrase         5
#define sInteger        6
#define sDot            7
#define sReal1          8
#define sReal2          9
#define sReal3          10
#define sReal4          11
#define sFault          12
#define sSymbol         13
#define sComplex        14
#define sComplex1       15
#define sComplex2       16
#define sComplex3       17
#define sComplex4       18
#define sComplex5       19
#define sComplex6       20
#define sComplex7       21
#define sDelim          22
#define sGets           23
#define sLower          24
#define sHigher         25
#define sUnequal        26
#define sMacron         27
#define sTildadot       28
#define sDash           29
#define sBool           30
#define sSChar          31
#define sFChar          32

#define sFinish         35
#define sAccept         36
#define sReplace        37

#define sError1         40
#define sError2         41
#define sError3         42
#define sError4         43
#define sError5         44
#define sError6         45
#define sError7         46
#define sError8         47
#define sError9         48


#define     St  0            /* State short forms. */
#define     I   1
#define     Co  2
#define     S   3
#define     S1  4
#define     P   5
#define     N   6
#define     D   7
#define     R1  8
#define     R2  9
#define     R3  10
#define     R4  11
#define     F   12
#define     Sy  13
#define     C0  14
#define     C1  15
#define     C2  16
#define     C3  17
#define     C4  18
#define     C5  19
#define     C6  20
#define     C7  21
#define     Dm  22
#define     Ge  23
#define     Lo  24
#define     Hi  25
#define     Ue  26
#define     M   27
#define     Td  28
#define     Da  29
#define     Bo  30
#define     Ch  31
#define     Fc  32

#define     Fi  35
#define     A   36
#define     Rp  37

#define     E1  40
#define     E2  41
#define     E3  42
#define     E4  43
#define     E5  44
#define     E6  45
#define     E7  46
#define     E8  47
#define     E9  48

/*  transition table  */

#define     StateCnt   33
#define     ClassCnt   26

/*  character class definitions  */

#define     cLetter      0
#define     cDigit       1
#define     cDelimiter   2
#define     cUnused      3
#define     cDot         4
#define     cUnderscore  5
#define     cFault       6
#define     cPhrase      7
#define     cQuote       8
#define     cComment     9
#define     cColon      10
#define     cEqual      11
#define     cEndOfLine  12
#define     cLetterE    13
#define     cBlank      14
#define     cTilda      15
#define     cDash       16
#define     cSemicolon  17
#define     cSymbol     18
#define     cLetterJ    19
#define     cLessthan   20
#define     cGrtrthan   21
#define     cLetterbool 22
#define     cGrave      23
#define     cChar       24
#define     cPlus       25

#define     L    0           /* Character class short forms. */
#define     Di   1
#define     De   2
#define     U    3
#define     Do   4
#define     Un   5
#define     Ft   6
#define     Ph   7
#define     Q    8
#define     C    9
#define     Cl  10
#define     Eq  11
#define     El  12
#define     Le  13
#define     Bl  14
#define     Ti  15
#define     Ds  16
#define     Se  17
#define     Sm  18
#define     Lj  19
#define     Lt  20
#define     Gt  21
#define     Lb  22
#define     cG  23
#define     cC  24
#define     Pl  25

#else

#define tok_ambi      0
#define tok_truth     1
#define tok_bits      2
#define tok_int       3
#define tok_real      4
#define tok_cmpl      5
#define tok_char      6
#define tok_phrase    7
#define tok_fault     8
#define tok_ident     9
#define tok_res      10
#define tok_str      11
#define tok_delim    12
#define tok_dot      13

#endif
