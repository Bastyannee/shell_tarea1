#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 64

typedef enum { RUNNING, STOPPED, DONE } JobState;

typedef struct {
    int job_id;
    pid_t pid;
    char cmd_name[256];
    JobState state;
    int active;
} Job;

extern Job jobs[MAX_JOBS];

void init_jobs(void);
int add_job(pid_t pid, const char *cmd_name);
void sigchld_handler(int sig);
void check_completed_jobs(void);
void list_jobs(void); // <-- Esta es la línea que falta

#endif /* JOBS_H */