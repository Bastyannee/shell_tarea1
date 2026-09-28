#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

// Definición global exportada hacia pmon.c
Job jobs[MAX_JOBS];
static int next_job_id = 1;

void init_jobs(void) {
    memset(jobs, 0, sizeof(jobs));
}

int add_job(pid_t pid, const char *cmd_name) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) {
            jobs[i].job_id = next_job_id++;
            jobs[i].pid = pid;
            strncpy(jobs[i].cmd_name, cmd_name, 255);
            jobs[i].state = RUNNING;
            jobs[i].active = 1;
            return jobs[i].job_id;
        }
    }
    return -1; // Tabla llena
}

void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno; // Respaldo crítico para no corromper el ciclo principal
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        for (int i = 0; i < MAX_JOBS; i++) {
            if (jobs[i].active && jobs[i].pid == pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    jobs[i].state = DONE;
                }
                break;
            }
        }
    }
    errno = saved_errno;
}

void check_completed_jobs(void) {
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active && jobs[i].state == DONE) {
            printf("[%d]+ Done\t\t%s\n", jobs[i].job_id, jobs[i].cmd_name);
            jobs[i].active = 0; // Liberar el slot
        }
    }

    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

void list_jobs(void) {
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active && jobs[i].state != DONE) {
            const char *state_str = (jobs[i].state == RUNNING) ? "Running" : "Stopped";
            printf("[%d]  %d %-10s %s\n", jobs[i].job_id, jobs[i].pid, state_str, jobs[i].cmd_name);
        }
    }
    
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}