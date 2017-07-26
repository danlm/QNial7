/*==============================================================

  HEADER FILEIO.H

  COPYRIGHT NIAL Systems Limited  1983-2005

  Header file for fileio.c

================================================================*/

/* Bit flags for use in nprintf */
enum {
OF_NORMAL = 2, OF_MESSAGE = 4, OF_DEBUG = 8, OF_LOG = 16};

/* convienence bit flags for use in nprintf */
/* _LOG is ignored so these could be simplified */
#define OF_NORMAL_LOG (OF_NORMAL + OF_LOG)
#define OF_MESSAGE_LOG (OF_MESSAGE + OF_LOG)
#define OF_DEBUG_LOG (OF_DEBUG + OF_LOG)

#define STDIN stdin
#define STDOUT stdout
#define STDERR stderr


/* prototypes from fileio.c */

extern int  nprintf(int flags, char *fmt,...);
extern void startfilesystem(void); /* used by lib_main.c mainlp.c */
extern void closeuserfiles(void); /* used by mainlp.c */
extern void pushsysfile(FILE * pn); /* used by wsmanage.c */
extern void popsysfile(void); /* used by wsmanage.c */
extern void cleardeffiles(void);
extern int  readfileline(FILE * fnum, int mode);
extern void readinput(void); /* used by eval.c  fileio.c */
extern void show(nialptr arr); 
      /* used by diag.c eval.c fileio.c mainlp.c wsmanage.c */
extern void writelog(char *text, int textl, int newlineflg);
      /* used by coreif.c fileio.c unixif.c win32if.c wsmanage.c */
extern void rl_gets (char *promptstr, char *inputline);
      /* used in main_stu.c  to get top level input */