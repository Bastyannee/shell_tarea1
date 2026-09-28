#ifndef JOBS_H
#define JOBS_H

#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include "parser.h"

typedef enum { JOB_RUNNING, JOB_DONE } JobState;

// Un proceso perteneciente a un job (un job = un pipeline en background)
typedef struct {
    pid_t pid;
    volatile sig_atomic_t alive;   // lo modifica el handler de SIGCHLD
    unsigned long prev_ticks;      // utime+stime en la muestra anterior (pmon)
    struct timespec prev_time;     // instante de la muestra anterior (pmon)
} JobProc;

typedef struct Job {
    int id;                            // [1], [2], ...
    pid_t pgid;                        // grupo de procesos (= pid del primer hijo)
    JobProc procs[MAX_CMDS];
    int nprocs;
    volatile sig_atomic_t remaining;   // procesos aún vivos
    volatile sig_atomic_t state;       // JobState
    volatile sig_atomic_t exit_status; // status crudo de waitpid del último proceso
    char cmdline[256];
    struct Job *next;                  // lista enlazada
} Job;

// Llamar con SIGCHLD bloqueado (evita carrera con el handler).
Job *jobs_add(const pid_t *pids, int n, pid_t pgid, const char *cmdline);

void jobs_handle_sigchld(int sig);  // handler: waitpid(-1, WNOHANG) en ciclo
void jobs_notify_done(void);        // imprime "[1]+ Done ..." y libera jobs terminados
void jobs_print(void);              // built-in "jobs"
int  jobs_running(void);            // cantidad de jobs en ejecución
Job *jobs_first(void);              // cabeza de la lista (para pmon)
void jobs_kill_all(void);           // SIGTERM a los jobs vivos y libera la lista

#endif
