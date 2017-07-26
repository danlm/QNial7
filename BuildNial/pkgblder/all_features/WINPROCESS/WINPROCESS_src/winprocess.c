/*==============================================================

  MODULE   WINPROCESS.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  Standard external process module for Nial
  
  Contributed by John Gibbons

================================================================*/


/* Q'Nial file that selects features */

#include "switches.h"

#ifdef WINNIAL
#ifdef WINPROCESS

#include <windows.h>

/* Standard library header files */

/* IOLIB */
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/fcntl.h>

/* LIMITLIB */
#include <limits.h>

/* STDLIB */
#include <stdlib.h>

/* STLIB */
#include <string.h>

/* SIGLIB */
#include <signal.h>

/* SJLIB */
#include <setjmp.h>

/* PROCESSLIB */
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>


/* Q'Nial header files */

#include "absmach.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"

#include <time.h>
#include <string.h>



/* ======================== Child Processes =========================== */


/**
 * Possible states for a child process
 */
#define ACTIVE_CHILD         1
#define TERMINATED_CHILD     2
#define FREE_CHILD           3

/*
 * Termination of child processes is handled by the SIGCHLD handler
 * and a child can either be managed or unmanaged. By default a process
 * is managed.
 * 
 * Running waitpid inside the signal handler avoids zombie processes.
 *
 * A managed child is one where the parent process wishes to be 
 * notified upon termination so that the Nial code can perform
 * some post processing. A typical example would be a child spawned 
 * to perform a computation in parallel.
 *
 * An unmanaged child is one where the parent process has no interest
 * in being notified and the resources can be reaped. The are reaped
 * when creating new processes or closing existing processes. 
 * 
 * A simple example of an unmanaged process would be to create a plot
 * while the parent continues its processing.
 *
 */ 
#define UNMANAGED_CHILD      0x001

/*
 * Child process structure
 */
typedef struct {
  HANDLE  child;
  int     child_status;
  int     termination_status;
  int     child_flags;
  HANDLE  rd_pipe;
  HANDLE  wr_pipe;
  PROCESS_INFORMATION procInfo;
} ChildProcess, *ChildProcPtr;


#define MAX_CHILDREN  1024
static int num_children = 0;
static ChildProcPtr children[MAX_CHILDREN];
static int mod_init = 0;



/**
 * Release resources associated with a child
 */
static void releaseChild(int i) {
  if (children[i] != NULL) {
    ChildProcPtr child = children[i];

    if (child->child != NULL) TerminateProcess(child->child, 255);
    if (child->rd_pipe != NULL) CloseHandle(child->rd_pipe);
    if (child->wr_pipe != NULL) CloseHandle(child->wr_pipe);

    free(child);
    children[i] = NULL;
  }
}



/**
 * Assign a child process slot
 */
static int assignChild(HANDLE rpd, HANDLE wpd) {
  int i;
  ChildProcPtr p;      

  /* Initialise the module if necessary */
  if (mod_init == 0) {

    /* Set up the children table */
    for (i = 0; i < MAX_CHILDREN; i++)
      children[i] = NULL;
   
    /* avoid further calls */
    mod_init = 1;
  }

  /* find a free slot */
  for (i = 0; i < MAX_CHILDREN; i++) {
    if (children[i] == NULL || 
        (children[i]->child_status == TERMINATED_CHILD && (children[i]->child_flags & UNMANAGED_CHILD))) {
      
      /* Clean up old resources */
      if (children[i] != NULL)
        releaseChild(i);
       
      /* Assign a fresh Child process */
      p = (ChildProcPtr)malloc(sizeof(ChildProcess));

      if (p == NULL) {
        return -1;
      } else {
        p->child_status = ACTIVE_CHILD;
        p->child   = NULL;
        p->rd_pipe = rpd;
        p->wr_pipe = wpd;
        children[i] = p;
        return i;
      }
    }
  }

  return -1;
}


#define VALID_CHILD(i) (mod_init == 1 && 0 <= i && i < MAX_CHILDREN && children[i] != NULL)
	

/**
 * Need this routine for subprocess creation.
 * A child process of an interactive process is not interactive
 */ 
static void set_non_interactive() {
  quiet = true;
  messages_on = false;
  debugging_on = false;
  triggered = false;
  nouserinterrupts = true;
  keeplog = false;
  return;
}


static void make_sys_error() {
    LPVOID lpMessageBuf;
    DWORD lastError = GetLastError();
    
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  lastError,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR)&lpMessageBuf,
                  0,
                  NULL);
    mknstring((LPSTR)lpMessageBuf, lstrlen((LPCSTR)lpMessageBuf));
    LocalFree(lpMessageBuf);
  return;
}


/**
 *
 */
static nialint child_proc_count(nialint child) {
  DWORD totalBytesAvail;
  
  if (PeekNamedPipe(children[child]->rd_pipe,
                    NULL,
                    0,
                    NULL,
                    &totalBytesAvail,
                    NULL)) {
    ULARGE_INTEGER count;
    
    memcpy(&count, &totalBytesAvail, sizeof(ULARGE_INTEGER));
    return (nialint)(count.QuadPart&0xFFFFFFFF);
  } else {
    return -1;
  }
}



/* ========================= Process Primitives ================= */

/**
 * Create a child process that will communicate through pipes
 *
 * run_process cmd
 *
 * Returns the index of the child 
 */
void
irun_child(void)
{
  nialptr x = apop();
  nialint child = -1, avail = 2048;
  HANDLE  childStdinRd = NULL, childStdinWr = NULL;
  HANDLE  childStdoutRd = NULL, childStdoutWr = NULL;
  SECURITY_ATTRIBUTES saAttr;
  STARTUPINFO siStartupInfo;
  BOOL success = FALSE;
  
  /* Ensure the flags parameter is a character type */
  if (!istext(x)) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }


  /* Get a child slot */
  child = assignChild(NULL, NULL);
  if (child < 0) {
    apush(makefault("?nomem"));
    freeup(x);
    return;
  }

  
  /* Arrange for pipes to be inherited */
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;
  
  /* Create child process' stdout */
  if (!CreatePipe(&childStdoutRd, &childStdoutWr, &saAttr, 0))
    goto sys_error_exit;
    
  /* Ensure read handle is not inherited */
  if (!SetHandleInformation(childStdoutRd, HANDLE_FLAG_INHERIT, 0))
    goto sys_error_exit;
  
  /* Create a pipe for the child processes stdin */
  if (!CreatePipe(&childStdinRd, &childStdinWr, &saAttr, 0))
    goto sys_error_exit;
    
  /* Ensure the write handle is not inherited */
  if (!SetHandleInformation(childStdinWr, HANDLE_FLAG_INHERIT, 0))
    goto sys_error_exit;
    
  /* Set up the starup information for the child */
  ZeroMemory(&children[child]->procInfo, sizeof(PROCESS_INFORMATION));
  ZeroMemory(&siStartupInfo, sizeof(STARTUPINFO));
  siStartupInfo.cb = sizeof(STARTUPINFO);
  siStartupInfo.hStdError = childStdoutWr;
  siStartupInfo.hStdOutput = childStdoutWr;
  siStartupInfo.hStdInput  = childStdinRd;
  siStartupInfo.dwFlags   |= STARTF_USESTDHANDLES;
  
  /* Create child */
  success = CreateProcess(NULL,
                          pfirstchar(x),    /* Command line */
                          NULL,             /* process security attributes */
                          NULL,             /* primary thread security attributes */
                          TRUE,             /* handles are inherited */
                          0,                /* creation flags */
                          NULL,             /* use parents environment */
                          NULL,             /* use parents current directory */
                          &siStartupInfo,   /* startup info pointer */
                          &children[child]->procInfo);
  
  if (!success) {
    goto sys_error_exit;
  } else {
    /* Save values in child structure and return */ 
    children[child]->child   = children[child]->procInfo.hProcess;
    children[child]->rd_pipe = childStdoutRd;
    children[child]->wr_pipe = childStdinWr;
  }  
  apush(createint(child));
  freeup(x);
  return;


  sys_error_exit:

    /* format a system error suitable for display */
    make_sys_error();
  
    /* Clean up any internal data structures */
    if (childStdoutWr != NULL) CloseHandle(childStdoutWr);
    if (childStdoutRd != NULL) CloseHandle(childStdoutRd);
    if (childStdinWr != NULL) CloseHandle(childStdinWr);
    if (childStdinRd != NULL) CloseHandle(childStdinRd);
    
    /* Free resources */
    releaseChild(child);
    freeup(x);
    return;
}

/* MAX_PROC_READ must be larger than MAX_LINE_READ */
#define MAX_PROC_READ  8192
#define MAX_LINE_READ  2048


static DWORD get_line_length(nialint child) {
  DWORD avail = child_proc_count(child);
  char cbuf[MAX_LINE_READ];
  DWORD countRead = 0;
  
  avail = (avail < MAX_LINE_READ)? avail: MAX_LINE_READ;
  if (avail <= 0) {
    return 0;
  } else if (PeekNamedPipe(children[child]->rd_pipe, cbuf, (DWORD)avail, &countRead, NULL, NULL)) {
    /* Try to find a line */
    DWORD llen = 0;
    
    /* Anything to work with */
    if (countRead <= 0)
      return 0;
    
    /* Look for a newline character */
    for (llen = 0; llen < countRead; llen++) {
      if (cbuf[llen] == '\n') {
        return llen+1;
      }
    }
    
    /* we reached the end of the buffer without a newline */
    return llen;
  } else {
    return -1;
  }
}



void ichild_read(void)
{
  nialptr x = apop();
  nialint child, count = 0, rdc, *iptr;
  ChildProcPtr p;
  char cbuf[MAX_PROC_READ+2];
  DWORD avail;
  
  if (kind(x) != inttype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  iptr = pfirstint(x);
  if (tally(x) > 1) {
    child = *iptr++;
    count = *iptr++;
  } else {
    child = *iptr++;
    count = 0;
  }
  
  if (count > MAX_PROC_READ) {
    apush(makefault("?limit"));
    freeup(x);
    return;
  }
  
  if (!VALID_CHILD(child)) {
    apush(makefault("?invalid process"));
    freeup(x);
    return;
  }
  
  p = children[child];
  avail = child_proc_count(child);
  
  if (count <= 0) {
    /*
     * Reading a line of characters. First we peek at the input to
     * see if there is a line. Then we read the line.
      */
    DWORD llen = get_line_length(child);
    DWORD countRead;
    
    /* Nothing available to read */
    if (llen <= 0) {
      apush(Null);
      freeup(x);
      return;
    }
    
    
    /* Try to read the line */
    if (!ReadFile(p->rd_pipe, cbuf, llen, &countRead, NULL)) {
      make_sys_error();
      freeup(x);
      return;
    }
    
    /* Return whatever we got */
    rdc = countRead;
    
 } else {
    /* Reading a block of characters */
    DWORD countRead;
    nialint avail = child_proc_count(child);
    
    avail = (avail < 2048)? avail: 2048;
    if (avail <= 0) {
      rdc = 0;
    } else if (!ReadFile(p->rd_pipe, &cbuf, (DWORD)avail, &countRead, NULL)) {
      make_sys_error();
      freeup(x);
      return;
    } else  {
      rdc = (nialint)countRead;
    }
  }

  if (rdc > 0) {
    mknstring(cbuf, rdc);
  } else {
    apush(Null);
  }

  freeup(x);
  return;
}


/**
 * Write to the input stream of the child process
 */
void ichild_write(void)
{
  nialptr x = apop();
  nialint child, count = 0, *iptr, res;
  nialptr cptr, dptr;
  ChildProcPtr p;
  DWORD toWrite, written;
  
  if (kind(x) != atype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  splitfb(x, &cptr, &dptr);
  if (kind(cptr) != inttype || kind(dptr) != chartype || !VALID_CHILD(intval(cptr))) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }
  
  p = children[intval(cptr)];
  toWrite = tally(dptr);
  if (WriteFile(p->wr_pipe, pfirstchar(dptr), toWrite, &written, NULL)) {
    apush(createint((nialint)written));
  } else {
    make_sys_error();
  }

  freeup(x);
  return;
}


void ichild_close(void)
{
  nialptr x = apop();
  nialint *px, child, exitcode = 255;
  int st;
  
  
  if (kind(x) != inttype || !VALID_CHILD(intval(x))) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  px = pfirstint(x);
  child = *px++;
  if (tally(x) == 2) exitcode = *px++;
  
  st = TerminateProcess(children[child]->child, exitcode);
  releaseChild(child);
  apush((st)? True_val: False_val);
  freeup(x);
  return;
}


static int child_is_running(nialint child)
{
  DWORD exitStatus;
  
  int st = GetExitCodeProcess(children[child]->child, &exitStatus);
  
  if (st) {
    return (exitStatus == STILL_ACTIVE)? 1: 0;
  } else {
    return -1;
  }
}


void ichild_running(void) {
  nialptr x = apop();
  nialint child;
  
  if (kind(x) != inttype || !VALID_CHILD(intval(x))) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }
  
  switch(child_is_running(intval(x))) {
    case 1:
      apush(True_val);
      break;
    case 0:
      apush(False_val);
      break;
    default:
      make_sys_error();
  }

  freeup(x);
  return;
}



/**
 * Return a count of the number of bytes available for reading
 */
void ichild_count(void) {
  nialptr x = apop();
  
  if (kind(x) == inttype && VALID_CHILD(intval(x))) {
    apush(createint(child_proc_count(intval(x))));
  } else {
    apush(makefault("?args"));
  }
  
  freeup(x);
  return;
}

#endif /* WINPROCESS */
#endif /* WINNIAL */




