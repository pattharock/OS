#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/resource.h>
#include <time.h>

#define R 20
#define C 20
#define BILLION 1000000000L;

int parentID = 0;

void catch_sig(int sig)
{
  if (getpid() == parentID)
  {
    signal(sig, SIG_DFL);
  }
};

int main(int argc, char *argv[])
{

  struct timespec monitor_start;
  struct timespec monitor_end;
  double smtime;
  double emtime;
  parentID = getpid();
  clock_gettime(CLOCK_REALTIME, &monitor_start);
  char *args[R][C];
  struct rusage monitorusage;
  for (int i = 0; i < R; i++)
  {
    for (int j = 0; j < C; j++)
    {
      args[i][j] = "";
    }
  }
  int r = 0;
  int c = 0;
  int P = 1;
  for (int i = 1; i < argc; i++)
  {
    if (strcmp("!", argv[i]) == 0)
    {
      P++;
      args[r][c] = NULL;
      r++;
      i++;
      c = 0;
    }
    args[r][c++] = argv[i];
  }
  args[r][c] = NULL;

  int child_process[P];
  int fd[P - 1][2];
  struct rusage usage[P];

  for (int i = 1; i <= NSIG; i++)
  {
    signal(i, catch_sig);
  }

  for (int i = 0; i < P - 1; i++)
  {
    if (pipe(fd[i]) == -1)
    {
      printf("pipe() failed\n");
      exit(-1);
    }
  }

  for (int i = 0; i < P; i++)
  {
    child_process[i] = fork();
    if (child_process[i] == -1)
    {
      printf("fork() failed\n");
      exit(-1);
    }

    if (child_process[i] == 0)
    {
      if (i == 0)
      {
        dup2(fd[i][1], STDOUT_FILENO);
        close(fd[i][1]);
      }
      else if (i == P - 1)
      {
        dup2(fd[i - 1][0], STDIN_FILENO);
        close(fd[i - 1][0]);
      }
      else
      {
        dup2(fd[i - 1][0], STDIN_FILENO);
        dup2(fd[i][1], STDOUT_FILENO);
        close(fd[i][1]);
        close(fd[i - 1][0]);
      }
      for (int i = 0; i < P - 1; i++)
      {
        close(fd[i][0]);
        close(fd[i][1]);
      }

      execvp(args[i][0], args[i]);
      printf("FAIL EXEC");
    }
    else
    {
      printf("\nProcess with id: %d created for the command: %s\n", child_process[i], args[i][0]);
    }
  }

  for (int i = 0; i < P - 1; i++)
  {
    close(fd[i][0]);
    close(fd[i][1]);
  }

  for (int i = 0; i < P; i++)
  {
    int status;
    struct timespec child_start;
    struct timespec child_end;
    clock_gettime(CLOCK_REALTIME, &child_start);
    int pid = wait4(child_process[i], &status, 0, &usage[i]);
    clock_gettime(CLOCK_REALTIME, &child_end);

    if (WIFSIGNALED(status))
    {
      int signal = WTERMSIG(status);
      printf("The command \"%s\" is interrupted by the signal number = %d (Signal: %s)\n", args[i][0], signal, strsignal(signal));
    }

    if (WIFEXITED(status))
    {
      if (WEXITSTATUS(status) == 0)
      {
        printf("\nThe command \"%s\" terminated with returned status code = %d\n", args[i][0], status);
      }
      else
      {
        printf("\nmonitor experienced an error in starting the command: %s \n", args[i][0]);
      }
    }

    double time_diff = (child_end.tv_sec - child_start.tv_sec) + (double)(child_end.tv_nsec - child_start.tv_nsec) / (double)BILLION;
    printf("real: %03fs, user: %ld.%03ds, system: %ld.%03ds\n", time_diff, usage[i].ru_utime.tv_sec, usage[i].ru_utime.tv_usec, usage[i].ru_stime.tv_sec, usage[i].ru_stime.tv_usec);
    printf("no. of page faults: %ld\n", usage[i].ru_minflt + usage[i].ru_majflt);
    printf("no. of context switches: %ld\n", usage[i].ru_nvcsw + usage[i].ru_nivcsw);
  }

  // time(&monitor_start);
  clock_gettime(CLOCK_REALTIME, &monitor_end);
  double d;
  d = (monitor_end.tv_sec - monitor_start.tv_sec) + (double)(monitor_end.tv_nsec - monitor_start.tv_nsec) / (double)BILLION;
  getrusage(RUSAGE_SELF, &monitorusage);
  printf("\nThe command \"%s\" terminated with returned status code = %d\n", argv[0], 0); // 0 is hardcoded
  printf("real: %03fs, user: %ld.%03ds, system: %ld.%03ds\n", d, monitorusage.ru_utime.tv_sec, monitorusage.ru_utime.tv_usec, monitorusage.ru_stime.tv_sec, monitorusage.ru_stime.tv_usec);
  printf("no. of page faults: %ld\n", monitorusage.ru_minflt + monitorusage.ru_majflt);
  printf("no. of context switches: %ld\n", monitorusage.ru_nvcsw + monitorusage.ru_nivcsw);
  return 0;
}
