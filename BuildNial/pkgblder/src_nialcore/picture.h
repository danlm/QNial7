/*==============================================================

  PICTURE.H:  header for PICTURE.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the macros and prototypes for the array drawing functions.

================================================================*/

extern void initformat(char *);
extern void initboxchars(int usedefault);
extern char stdformat[20];


#define lft 0                /* Types for justification for pictures */
#define rght 1
#define centered 2




/* defines the box characters on the IBMPC */

#define  ibm_luc     218
#define  ibm_ruc     191
#define  ibm_llc     192
#define  ibm_rlc     217
#define  ibm_ut      194
#define  ibm_lt      193
#define  ibm_rt      180
#define  ibm_gt      195
#define  ibm_cro     197
#define  ibm_hor     196
#define  ibm_ver     179

#define     ibm_wluc   201
#define     ibm_wruc   187
#define     ibm_wllc   200
#define     ibm_wrlc   188
#define     ibm_whor   205
#define     ibm_wver   186


#define     def_luc   '+'
#define     def_ruc   '+'
#define     def_llc   '+'
#define     def_rlc   '+'
#define      def_ut   '+'
#define      def_lt   '+'
#define      def_rt   '+'
#define      def_gt   '+'
#define     def_cro   '+'
#define     def_hor   '-'
#define     def_ver   '|'

#define     def_wluc   luc
#define     def_wruc   ruc
#define     def_wllc   llc
#define     def_wrlc   rlc
#define     def_whor   hor
#define     def_wver   ver




/* functions to get and set the format string */

char       *getformat(void);
void        setformat(char *newformat);
