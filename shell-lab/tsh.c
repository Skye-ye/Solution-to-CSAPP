/*
 * tsh - A tiny shell program with job control
 *
 * <Put your name and login ID here>
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */
volatile sig_atomic_t fg_pid;

struct job_t {           /* The job struct */
  pid_t pid;             /* job PID */
  int jid;               /* job ID [1, 2, ...] */
  int state;             /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE]; /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
void Execve(const char *filename, char *const argv[], char *const envp[]);
pid_t Wait(int *status);
pid_t Waitpid(pid_t pid, int *iptr, int options);
void Kill(pid_t pid, int signum);
pid_t Fork();
void Pause();
unsigned int Sleep(unsigned int secs);
unsigned int Alarm(unsigned int seconds);
void Setpgid(pid_t pid, pid_t pgid);
pid_t Getpgrp(void);
handler_t *Signal(int signum, handler_t *handler);
void Sigprocmask(int how, sigset_t *set, sigset_t *oldset);
void Sigfillset(sigset_t *set);
void Sigemptyset(sigset_t *set);
void Sigaddset(sigset_t *set, int signum);
void Sigdelset(sigset_t *set, int signum);
int Sigismember(const sigset_t *set, int signum);
int Sigsuspend(const sigset_t *set);
void Sio_error(char s[]);
ssize_t Sio_puts(char s[]);
ssize_t Sio_putl(long v);

/*
 * main - The shell's main routine
 */
int main(int argc, char **argv) {
  char c;
  char cmdline[MAXLINE];
  int emit_prompt = 1; /* emit prompt (default) */

  /* Redirect stderr to stdout (so that driver will get all output
   * on the pipe connected to stdout) */
  dup2(1, 2);

  /* Parse the command line */
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h': /* print help message */
      usage();
      break;
    case 'v': /* emit additional diagnostic info */
      verbose = 1;
      break;
    case 'p':          /* don't print a prompt */
      emit_prompt = 0; /* handy for automatic testing */
      break;
    default:
      usage();
    }
  }

  /* Install the signal handlers */

  /* These are the ones you will need to implement */
  Signal(SIGINT, sigint_handler);   /* ctrl-c */
  Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
  Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

  /* This one provides a clean way to kill the shell */
  Signal(SIGQUIT, sigquit_handler);

  /* Initialize the job list */
  initjobs(jobs);

  /* Execute the shell's read/eval loop */
  while (1) {
    /* Read command line */
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }
    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
      app_error("fgets error");
    if (feof(stdin)) { /* End of file (ctrl-d) */
      fflush(stdout);
      exit(0);
    }

    /* Evaluate the command line */
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  }

  exit(0); /* control never reaches here */
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */
void eval(char *cmdline) {
  char *argv[MAXARGS]; /* Argument list execve() */
  char buf[MAXLINE];   /* Holds modified command line */
  int bg;              /* Should the job run in bg or fg? */
  pid_t pid;           /* Process id */

  sigset_t mask_all, mask_one, prev_one;

  Sigfillset(&mask_all);
  Sigemptyset(&mask_one);
  Sigaddset(&mask_one, SIGCHLD);

  strcpy(buf, cmdline);
  bg = parseline(buf, argv);
  if (argv[0] == NULL)
    return; /* Ignore empty lines */

  if (!builtin_cmd(argv)) {
    Sigprocmask(SIG_BLOCK, &mask_one, &prev_one); /* Block SIGCHLD */
    if ((pid = Fork()) == 0) {                    /* Child runs user job */
      Setpgid(0, 0); /* Put the child in a new process group */
      Signal(SIGINT, SIG_DFL);
      Signal(SIGTSTP, SIG_DFL);
      Sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD */
      execve(argv[0], argv, environ);
      printf("%s: Command not found\n", argv[0]);
      exit(0);
    }

    Setpgid(pid, pid);

    /* Parent waits for foreground job to terminate */
    if (!bg) {
      Sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Block all signals */
      addjob(jobs, pid, FG, cmdline);
      fg_pid = 0;
      while (!fg_pid) {
        Sigsuspend(&prev_one);
      }
      Sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD */
    } else {
      Sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Block all signals */
      addjob(jobs, pid, BG, cmdline);
      Sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD */

      printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
    }
  }
  return;
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char *cmdline, char **argv) {
  static char array[MAXLINE]; /* holds local copy of command line */
  char *buf = array;          /* ptr that traverses command line */
  char *delim;                /* points to first space delimiter */
  int argc;                   /* number of args */
  int bg;                     /* background job? */

  strcpy(buf, cmdline);
  buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* ignore leading spaces */
    buf++;

  /* Build the argv list */
  argc = 0;
  if (*buf == '\'') {
    buf++;
    delim = strchr(buf, '\'');
  } else {
    delim = strchr(buf, ' ');
  }

  while (delim) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* ignore spaces */
      buf++;

    if (*buf == '\'') {
      buf++;
      delim = strchr(buf, '\'');
    } else {
      delim = strchr(buf, ' ');
    }
  }
  argv[argc] = NULL;

  if (argc == 0) /* ignore blank line */
    return 1;

  /* should the job run in the background? */
  if ((bg = (*argv[argc - 1] == '&')) != 0) {
    argv[--argc] = NULL;
  }
  return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
int builtin_cmd(char **argv) {
  if (!strcmp(argv[0], "quit"))
    exit(0);                      /* quit command */
  if (!strcmp(argv[0], "jobs")) { /* jobs command */
    listjobs(jobs);
    return 1;
  }
  if (!strcmp(argv[0], "bg") || !strcmp(argv[0], "fg")) {
    do_bgfg(argv);
    return 1;
  }
  return 0; /* not a builtin command */
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) {
  struct job_t *job;
  pid_t pid;

  if (argv[1] == NULL) {
    printf("%s command requires PID or %%jobid argument\n", argv[0]);
    return;
  }

  if (argv[1][0] != '%' && !isdigit(argv[1][0])) {
    printf("%s: argument must be a PID or %%jobid\n", argv[0]);
    return;
  }

  if (argv[1][0] == '%') {
    int jid = atoi(&argv[1][1]);
    job = getjobjid(jobs, jid);
    if (job == NULL) {
      printf("%s: No such job\n", argv[1]);
      return;
    }
  } else {
    pid = atoi(argv[1]);
    job = getjobpid(jobs, pid);
    if (job == NULL) {
      printf("(%d): No such process\n", pid);
      return;
    }
  }

  pid = job->pid;

  if (!strcmp(argv[0], "bg") && job->state == ST) {
    job->state = BG;
    Kill(-pid, SIGCONT);
    printf("[%d] (%d) %s", job->jid, pid, job->cmdline);
  } else if (!strcmp(argv[0], "fg") && job->state != FG) {
    if (job->state == ST) {
      Kill(-pid, SIGCONT);
    }
    job->state = FG;
    waitfg(pid);
  }

  return;
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid) {
  sigset_t mask;
  Sigemptyset(&mask);
  fg_pid = 0;
  while (fg_pid != pid) {
    Sigsuspend(&mask);
  }
  return;
}

/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
void sigchld_handler(int sig) {
  int olderrno = errno;
  sigset_t mask_all, prev_all;
  int status;

  Sigfillset(&mask_all);
  fg_pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
  Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

  if (WIFSTOPPED(status)) {
    struct job_t *job = getjobpid(jobs, fg_pid);
    if (job != NULL) {
      job->state = ST;

      Sio_puts("Job [");
      Sio_putl((long)job->jid);
      Sio_puts("] (");
      Sio_putl((long)job->pid);
      Sio_puts(") Stopped by signal ");
      Sio_putl((long)SIGTSTP);
      Sio_puts("\n");
    }
  } else {
    if (WIFSIGNALED(status)) {
      struct job_t *job = getjobpid(jobs, fg_pid);
      if (job != NULL) {
        int jid = job->jid;
        pid_t job_pid = job->pid;

        Sio_puts("Job [");
        Sio_putl((long)jid);
        Sio_puts("] (");
        Sio_putl((long)job_pid);
        Sio_puts(") terminated by signal ");
        Sio_putl((long)WTERMSIG(status));
        Sio_puts("\n");
      }
    }
    deletejob(jobs, fg_pid);
  }

  Sigprocmask(SIG_SETMASK, &prev_all, NULL);
  errno = olderrno;
  return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig) {
  int olderrno = errno;
  sigset_t mask_all, prev_all;
  pid_t pid;
  struct job_t *job;

  Sigfillset(&mask_all);
  Sigprocmask(SIG_BLOCK, &mask_all, &prev_all); /* Block all signals */

  pid = fgpid(jobs);
  if (pid) {
    job = getjobpid(jobs, pid);
    if (job != NULL) {
      Kill(-pid, sig);
    }
  }

  Sigprocmask(SIG_SETMASK, &prev_all, NULL); /* Unblock signals */
  errno = olderrno;
  return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig) {
  int olderrno = errno;
  sigset_t mask_all, prev_all;
  pid_t pid;
  struct job_t *job;

  Sigfillset(&mask_all);
  Sigprocmask(SIG_BLOCK, &mask_all, &prev_all); /* Block all signals */

  pid = fgpid(jobs);
  if (pid) {
    job = getjobpid(jobs, pid);
    if (job != NULL) {
      Kill(-pid, sig);
    }
  }

  Sigprocmask(SIG_SETMASK, &prev_all, NULL); /* Unblock signals */
  errno = olderrno;
  return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
  job->pid = 0;
  job->jid = 0;
  job->state = UNDEF;
  job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) {
  int i, max = 0;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid > max)
      max = jobs[i].jid;
  return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) {
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == 0) {
      jobs[i].pid = pid;
      jobs[i].state = state;
      jobs[i].jid = nextjid++;
      if (nextjid > MAXJOBS)
        nextjid = 1;
      strcpy(jobs[i].cmdline, cmdline);
      if (verbose) {
        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid,
               jobs[i].cmdline);
      }
      return 1;
    }
  }
  printf("Tried to create too many jobs\n");
  return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) {
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == pid) {
      clearjob(&jobs[i]);
      nextjid = maxjid(jobs) + 1;
      return 1;
    }
  }
  return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].state == FG)
      return jobs[i].pid;
  return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
  int i;

  if (pid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid)
      return &jobs[i];
  return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) {
  int i;

  if (jid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid == jid)
      return &jobs[i];
  return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) {
  int i;

  if (pid < 1)
    return 0;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid) {
      return jobs[i].jid;
    }
  return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0) {
      printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
      switch (jobs[i].state) {
      case BG:
        printf("Running ");
        break;
      case FG:
        printf("Foreground ");
        break;
      case ST:
        printf("Stopped ");
        break;
      default:
        printf("listjobs: Internal error: job[%d].state=%d ", i, jobs[i].state);
      }
      printf("%s", jobs[i].cmdline);
    }
  }
}
/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) {
  printf("Usage: shell [-hvp]\n");
  printf("   -h   print this message\n");
  printf("   -v   print additional diagnostic information\n");
  printf("   -p   do not emit a command prompt\n");
  exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg) {
  fprintf(stdout, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg) {
  fprintf(stdout, "%s\n", msg);
  exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) {
  struct sigaction action, old_action;

  action.sa_handler = handler;
  sigemptyset(&action.sa_mask); /* block sigs of type being handled */
  action.sa_flags = SA_RESTART; /* restart syscalls if possible */

  if (sigaction(signum, &action, &old_action) < 0)
    unix_error("Signal error");
  return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) {
  printf("Terminating after receipt of SIGQUIT signal\n");
  exit(1);
}

void Execve(const char *filename, char *const argv[], char *const envp[]) {
  if (execve(filename, argv, envp) < 0)
    unix_error("Execve error");
}

pid_t Wait(int *status) {
  pid_t pid;
  if ((pid = wait(status)) < 0)
    unix_error("Wait error");
  return pid;
}

pid_t Waitpid(pid_t pid, int *iptr, int options) {
  pid_t retpid;
  if ((retpid = waitpid(pid, iptr, options)) < 0)
    unix_error("Waitpid error");
  return retpid;
}

void Kill(pid_t pid, int signum) {
  int rc;
  if ((rc = kill(pid, signum)) < 0)
    unix_error("Kill error");
}

pid_t Fork() {
  pid_t pid;
  if ((pid = fork()) < 0)
    unix_error("Fork error");
  return pid;
}

void Pause() {
  (void)pause();
  return;
}

unsigned int Sleep(unsigned int secs) {
  unsigned int rc;

  if ((rc = sleep(secs)) < 0)
    unix_error("Sleep error");
  return rc;
}

unsigned int Alarm(unsigned int seconds) { return alarm(seconds); }

void Setpgid(pid_t pid, pid_t pgid) {
  int rc;

  if ((rc = setpgid(pid, pgid)) < 0)
    unix_error("Setpgid error");
  return;
}

pid_t Getpgrp(void) { return getpgrp(); }

void Sigprocmask(int how, sigset_t *set, sigset_t *oldset) {
  if (sigprocmask(how, set, oldset) < 0)
    unix_error("Sigprocmask error");
}

void Sigfillset(sigset_t *set) {
  if (sigfillset(set) < 0)
    unix_error("Sigfillset error");
}

void Sigemptyset(sigset_t *set) {
  if (sigemptyset(set) < 0)
    unix_error("Sigemptyset error");
}

void Sigaddset(sigset_t *set, int signum) {
  if (sigaddset(set, signum) < 0)
    unix_error("Sigaddset error");
}

void Sigdelset(sigset_t *set, int signum) {
  if (sigdelset(set, signum) < 0)
    unix_error("Sigdelset error");
  return;
}

int Sigismember(const sigset_t *set, int signum) {
  int rc;
  if ((rc = sigismember(set, signum)) < 0)
    unix_error("Sigismember error");
  return rc;
}

int Sigsuspend(const sigset_t *set) {
  int rc = sigsuspend(set); /* always returns -1 */
  if (errno != EINTR)
    unix_error("Sigsuspend error");
  return rc;
}

static void sio_reverse(char s[]) {
  int c, i, j;

  for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

/* sio_ltoa - Convert long to base b string (from K&R) */
static void sio_ltoa(long v, char s[], int b) {
  int c, i = 0;
  int neg = v < 0;

  if (neg)
    v = -v;

  do {
    s[i++] = ((c = (v % b)) < 10) ? c + '0' : c - 10 + 'a';
  } while ((v /= b) > 0);

  if (neg)
    s[i++] = '-';

  s[i] = '\0';
  sio_reverse(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[]) {
  int i = 0;

  while (s[i] != '\0')
    ++i;
  return i;
}
/* $end sioprivate */

/* Public Sio functions */
/* $begin siopublic */

ssize_t sio_puts(char s[]) /* Put string */
{
  return write(STDOUT_FILENO, s, sio_strlen(s)); // line:csapp:siostrlen
}

ssize_t sio_putl(long v) /* Put long */
{
  char s[128];

  sio_ltoa(v, s, 10); /* Based on K&R itoa() */ // line:csapp:sioltoa
  return sio_puts(s);
}

void sio_error(char s[]) /* Put error message and exit */
{
  sio_puts(s);
  _exit(1); // line:csapp:sioexit
}
/* $end siopublic */

/*******************************
 * Wrappers for the SIO routines
 ******************************/
ssize_t Sio_putl(long v) {
  ssize_t n;

  if ((n = sio_putl(v)) < 0)
    sio_error("Sio_putl error");
  return n;
}

ssize_t Sio_puts(char s[]) {
  ssize_t n;

  if ((n = sio_puts(s)) < 0)
    sio_error("Sio_puts error");
  return n;
}

void Sio_error(char s[]) { sio_error(s); }