/*==============================================================

  MODULE   SPROCESS.C

  COPYRIGHT NIAL Systems Limited  1983-2016

  Standard external process module for Nial
  
  Contributed by John Gibbons

================================================================*/

/* Q'Nial file that selects features */

#include "switches.h"

#ifdef SPROCESS

#ifdef LINUX
#include <pty.h>
#endif

#ifdef OSX
#include <util.h>
#endif

#include <signal.h>

#ifdef LINUX
#include <wait.h>
#endif
#ifdef OSX
#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/wait.h>
#endif
#include <sys/socket.h>

/* Standard library header files */

/* IOLIB */
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
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
#include <pwd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>


/* Q'Nial header files */

#include "absmach.h"
#include "qniallim.h"
#include "lib_main.h"
#include "ops.h"
#include "trs.h"
#include "fileio.h"

#include <time.h>
#include "nstreams.h"
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
  pid_t   child;
  int     child_status;
  int     termination_status;
  int     child_flags;
  nialint rd_stream;
  nialint wr_stream;
} ChildProcess, *ChildProcPtr;


#define MAX_CHILDREN  1024
static int num_children = 0;
static ChildProcPtr children[MAX_CHILDREN];
static int mod_init = 0;


/**
 * Signal handler for child termination
 */
static void handle_sigchld(int sig) {
  pid_t child_pid;
  int i, status;

  while ((child_pid = waitpid((pid_t)(-1), &status, WNOHANG)) > 0) {
    /* Deal with the terminated child */
    for (i = 0; i < MAX_CHILDREN; i++) 
      if (children[i] != NULL && children[i]->child == child_pid) {
        children[i]->child_status = TERMINATED_CHILD;
        children[i]->termination_status = status;
      }
  }

  return;
}


/**
 * Release resources associated with a child
 */
static void releaseChild(int i) {
  if (children[i] != NULL) {
    ChildProcPtr child = children[i];

    nio_freeStream(children[i]->rd_stream);
    nio_freeStream(children[i]->wr_stream);

    free(children[i]);
    children[i] = NULL;
  }
}



/**
 * Assign a child process slot
 */
static int assignChild(int rfd, int wfd) {
  int i;
  struct sigaction sa;
  ChildProcPtr p;
      

  /* Initialise the module if necessary */
  if (mod_init == 0) {
    /* Set up handler for SIGCHLD */
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
      /* Need to improve this piece */
      fprintf(stderr, "*** Failed to set handler for SIGCHLD ***\n");
      fflush(stderr);
    }

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
	int ios_rd = nio_createStream();
	int ios_wr = nio_createStream();
	if (ios_rd == -1 || ios_wr == -1) {
	  free(p);
	  if (ios_rd != -1) nio_freeStream(ios_rd);
	  if (ios_wr != -1) nio_freeStream(ios_wr);
	  return -1;
	}

        p->child_status = ACTIVE_CHILD;
	p->rd_stream = ios_rd;
	p->wr_stream = ios_wr;

	nio_set_fd(ios_rd, rfd);
	nio_set_fd(ios_wr, wfd);

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


/* ========================= Process Primitives ================= */

/**
 * Spawn a Nial child process and communicate through pipes
 *
 * spawn_child flags
 *
 * Returns the index of the child 
 */
void
ispawn_child(void)
{
  nialptr x = apop();
  nialint child;
  pid_t   childpid;
  int     pipefd1[2], pipefd2[2];

  /* Ensure the flags parameter is an integer */
  if (kind(x) != inttype) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }


  /* Get a child slot */
  child = assignChild(-1, -1);
  if (child < 0) {
    apush(makefault("?nomem"));
    freeup(x);
    return;
  }

  /* Create the pipes */
  if (pipe(pipefd1) < 0 || pipe(pipefd2) < 0) {
    apush(makefault("?syserror"));
    freeup(x);
    return;
  }

  /* fork the process */
  if ((childpid = fork()) < 0) {
    close(pipefd1[0]);
    close(pipefd1[1]);
    close(pipefd2[0]);
    close(pipefd2[1]);

    releaseChild(child);

    apush(makefault("?syserror"));
    freeup(x);
    return;
  }

 
  /* Now decide where we are */
  if (childpid > 0) {
    /* in parent */

    /* close redundant fd */
    close(pipefd1[0]);
    close(pipefd2[1]);

    /* return child index */
    children[child]->child_flags = intval(x);
    children[child]->child   = childpid;
    nio_set_fd(children[child]->wr_stream, pipefd1[1]);
    nio_set_mode(children[child]->wr_stream, IOS_LINE);
    nio_set_fd(children[child]->rd_stream, pipefd2[0]);
    nio_set_flags(children[child]->rd_stream, IOS_CHARSTREAM);
    apush(createint(child));
    freeup(x);
    return;

  } else {
    /* in child */

    close(pipefd1[1]);
    close(pipefd2[0]);

    /* Reset the standard input and output */
    dup2(pipefd1[0], 0);
    dup2(pipefd2[1], 1);
    dup2(pipefd2[1], 2);

    close(pipefd1[0]);
    close(pipefd2[1]);

    set_non_interactive();

    /*
    printf("Nial worker process started\n");
    fflush(stdout);
    */

    apush(Null);
    freeup(x);
    return;
  }

}


/**
 * Spawn an interactive shell process and communicate through pty
 *
 * spawn_shell flags
 *
 * Returns an integer identifying the child 
 */
void
ispawn_shell(void)
{
  nialptr x = apop();
  nialint child;
  pid_t   childpid;
  int     fd;

  /* Check type of flags argument */
  if (kind(x) != inttype) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }


  /* Get a child slot */
  child = assignChild(-1, -1);
  if (child < 0) {
    apush(makefault("?nomem"));
    freeup(x);
    return;
  }


  childpid = forkpty(&fd, NULL, NULL, NULL);

  /* Now decide where we are */
  if (childpid == -1) {
    apush(makefault("?forkpty"));
    freeup(x);
    return;
  } if (childpid > 0) {
    /* in parent */
    
    /* return child index */
    children[child]->child_flags = intval(x); 
    children[child]->child   = childpid;
    nio_set_fd(children[child]->wr_stream, fd);
    nio_set_mode(children[child]->wr_stream, IOS_LINE);
    nio_set_fd(children[child]->rd_stream, fd);
    nio_set_flags(children[child]->rd_stream, IOS_CHARSTREAM);
    
    apush(createint(child));
    freeup(x);
    return;

  } else {
    /* in child, execute a shell */
    if (execlp("/bin/sh", "sh", (void*)0) == -1) {
      perror("execlp");
    }
    fprintf(stderr, "shell exited.\n");
    fflush(stderr);
    return;
  }
}


/**
 * Spawn a command process and communicate through pipes
 *
 * spawn_cmd app args flags
 *
 * Returns an integer identifying the child 
 */
void
ispawn_cmd(void)
{
  nialptr x = apop();
  nialptr progname, progargs, flags;
  nialint i;
  nialint child;
  pid_t   childpid;
  int     pipefd1[2], pipefd2[2];
 
  /* Get a child slot */
  child = assignChild(-1, -1);
  if (child < 0) {
    apush(makefault("?nomem"));
    freeup(x);
    return;
  }

  /* Check argument types */
  if (kind(x) != atype || tally(x) != 3) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  /* Separate out args and chcek types */
  progname = fetch_array(x, 0);
  progargs = fetch_array(x, 1);
  flags    = fetch_array(x, 2);

  if (kind(progname) != chartype || kind(progargs) != atype || kind(flags) != inttype) {
    apush(makefault("?arg_types"));
    freeup(x);
    return;
  }

  /* Check args array */
  for (i = 0; i < tally(progargs); i++) {
    nialptr a = fetch_array(progargs, i);
    if (kind(a) != chartype) {
      apush(makefault("?invalid_argv"));
      freeup(x);
      return;
    }
  }

  /* Create the pipes */
  if (pipe(pipefd1) < 0 || pipe(pipefd2) < 0) {
    apush(makefault("?syserror"));
    freeup(x);
    return;
  }

  /* fork the process */
  if ((childpid = vfork()) < 0) {
    close(pipefd1[0]);
    close(pipefd1[1]);
    close(pipefd2[0]);
    close(pipefd2[1]);

    releaseChild(child);

    apush(makefault("?syserror"));
    freeup(x);
    return;
  }

 
  /* Now decide where we are */
  if (childpid > 0) {
    /* in parent */
    
    /* close redundant fd */
    close(pipefd1[0]);
    close(pipefd2[1]);

    /* return child index */
    children[child]->child   = childpid;
    children[child]->child_flags = intval(flags);
    nio_set_fd(children[child]->wr_stream, pipefd1[1]);
    nio_set_mode(children[child]->wr_stream, IOS_LINE);
    nio_set_fd(children[child]->rd_stream, pipefd2[0]);
    nio_set_flags(children[child]->rd_stream, IOS_CHARSTREAM);
    
    apush(createint(child));
    freeup(x);
    return;

  } else {
    /* in child */
    extern char **nial_envp;
    char *prg = pfirstchar(progname);
    char **args = (char **)malloc((tally(progargs)+1)*sizeof(char*));
    nialint i;

    close(pipefd1[1]);
    close(pipefd2[0]);

    dup2(pipefd1[0], 0);
    dup2(pipefd2[1], 1);
    dup2(pipefd2[1], 2);

    close(pipefd1[0]);
    close(pipefd2[1]);

    for (i = 0; i < tally(progargs); i++) {
      nialptr a = fetch_array(progargs, i);
      args[i] = pfirstchar(a);
    }
    args[i] = NULL;

    if (execve(prg, args, nial_envp) < 0) {
      perror("failed to exec child");
      exit(1);
    }
  }

}


/**
 * Return a stream for sending text to the child
 */
void ichild_writer(void) {
  nialptr x = apop();
  nialint ios = -1;

  if (kind(x) != inttype || !VALID_CHILD(intval(x))) { 
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  ios = children[intval(x)]->wr_stream;
  apush(createint(ios));
  freeup(x);
  return;
}


/**
 * Return a stream to receive the output of the child process
 */
void ichild_reader(void) {
  nialptr x = apop();
  nialint ios = -1;

  if (kind(x) != inttype || !VALID_CHILD(intval(x))) { 
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  ios = children[intval(x)]->rd_stream;
  apush(createint(ios));
  freeup(x);
  return;
}


/**
 * Send an interrupt signal to a child
 *
 * interrupt_child child sig
 *
 *  0 ->  SIGINT
 * -1 ->  SIGKILL
 */
void iinterrupt_child(void) {
  nialptr x = apop();
  ChildProcPtr ios;
  nialint child, sig;

  /* Check the arguments */
  if (kind(x) != inttype || tally(x) != 2) { 
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  child = fetch_int(x, 0);
  sig   = fetch_int(x, 1);

  if (!VALID_CHILD(child)) { 
    apush(makefault("?invalid_child"));
    freeup(x);
    return;
  }

  ios = children[child];

  if (sig >= 0) {
    /* handle non-kill signals */
    apush(createint(kill(ios->child, SIGINT)));
  } else {
    /* kill signals */
    apush(createint(kill(ios->child, SIGKILL)));
  }

  freeup(x);
  return;
}


/**
 * Return the current status of a nominated child (arg >= 0) or any
 * terminated managed child (arg < 0).
 */
void ichild_status(void) {
  nialptr x = apop();
  nialint *iptr, reslen = 3;
  nialptr res;
  nialint child;


  /* validate argument type */
  if (kind(x) != inttype) {
    apush(makefault("?arg_type"));
    freeup(x);
    return;
  }

  child = intval(x);

  if (child < 0) {
    /* Return any terminated managed child */
    int status = -1;
    int termination_status = -1;
    
    /* find the child */
    for (child = 0; child < MAX_CHILDREN; child++) 
      if (children[child] != NULL &&
          children[child]->child_status == TERMINATED_CHILD && 
          (children[child]->child_flags & UNMANAGED_CHILD) == 0) {
        status = children[child]->child_status;
	termination_status = children[child]->termination_status;
        releaseChild(child);
        break;
      }

    if (child == MAX_CHILDREN)
      child = -1;
    
    /* Return the details */
    res = new_create_array(inttype, 1, 0, &reslen);
    iptr = pfirstint(res);
    *iptr++ = (nialint)child;
    *iptr++ = (nialint)status;
    *iptr++ = (nialint)termination_status;
    
    apush(res);
    
  } else {
    /* Ensure we have a valid child */
    if (!VALID_CHILD(child)) {
      apush(makefault("?invalid_child"));
      freeup(x);
      return;
    }

    /* Create a return array */
    res = new_create_array(inttype, 1, 0, &reslen);
    iptr = pfirstint(res);
    
    /* Set return data */
    *iptr++ = child;
    *iptr++ = children[child]-> child_status;
    *iptr++ = children[child]-> termination_status;
    
    /* return */
    apush(res);
    
  }

  freeup(x);
  return;
}
  

/* ---------------------- fast timer/sleep ----------------- */


/**
 * Return a nanoseconds time value for more precise duration
 * calculations. The values returned by this function may
 * depend on the OS in use. 
 */ 
static double nano_time() {

#ifdef LINUX
  struct timespec val;
  if (clock_gettime(CLOCK_REALTIME, &val) == -1) {
    return -1.0;
  } else {
    return (val.tv_sec + val.tv_nsec*1.0e-9);
  }
#endif
#ifdef OSX
  /* Time is process time */
  static int mnt_initialised = 0;
  static double timer_base;

  if (!mnt_initialised) {
    mach_timebase_info_data_t tb = { 0 };
    mach_timebase_info(&tb);
    timer_base = tb.numer;
    timer_base /= tb.denom;
    mnt_initialised = 1;
  }

  return (mach_absolute_time()/1.0E9)*timer_base;
#endif
}


void inano_time(void) {
  nialptr x = apop(); 

  apush(createreal(nano_time()));
  freeup(x);
  return;
}



/**
 * allow for a nanosecond sleep duration of the current thread.
 * If the call is interrupted this will return the remaining time
 * otherwise 0.0
 */
static double nano_sleep(int secs, int nsecs) {
  struct timespec val;
  struct timespec res;

  val.tv_sec = secs;
  val.tv_nsec = nsecs;
  if (nanosleep(&val, &res) == -1) {
    return (res.tv_sec + res.tv_nsec*1.0e-9);
  } else {
    return 0.0;
  }

}


void inano_sleep(void) {
  nialptr x = apop(); 
  nialint isecs, insecs, *iptr;

  if (kind(x) != inttype || tally(x) != 2) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  iptr = pfirstint(x);
  isecs = *iptr++;
  insecs = *iptr++;

  apush(createreal(nano_sleep(isecs, insecs)));
  freeup(x);
  return;
}


/* ---------------------- Basic system calls -------------------- */


/**
 * allow a Nial process to exit indicating to its parent
 * the reason for its termination
 */
void isys_exit(void) {
  nialptr x = apop();
  
  if (kind(x) != inttype) {
    apush(makefault("?args"));
    freeup(x);
    return;
  }

  /* Perform a standard unix exit using the code supplied */
  exit((int)(intval(x)&0377));

  /* This should not happen */
  apush(makefault("?system_error"));
  freeup(x);
  return;
}


#endif /* SPROCESS */




