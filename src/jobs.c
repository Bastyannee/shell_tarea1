#include "jobs.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static Job *jobs_head = NULL;

Job *jobs_first(void) {
    return jobs_head;
}

/* ---- utilidades para bloquear SIGCHLD mientras se toca la lista ---- */

static void block_sigchld(sigset_t *old) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &set, old);
}

static void restore_mask(const sigset_t *old) {
    sigprocmask(SIG_SETMASK, old, NULL);
}

/* ---- API ---- */

Job *jobs_add(const pid_t *pids, int n, pid_t pgid, const char *cmdline) {
    Job *j = calloc(1, sizeof(Job));
    if (!j) {
        perror("calloc");
        return NULL;
    }

    int max_id = 0;
    for (Job *k = jobs_head; k; k = k->next)
        if (k->id > max_id) max_id = k->id;

    j->id = max_id + 1;
    j->pgid = pgid;
    j->nprocs = n;
    j->remaining = n;
    j->state = JOB_RUNNING;
    snprintf(j->cmdline, sizeof(j->cmdline), "%s", cmdline);

    /* prev_time = instante de creacion: la primera muestra de pmon calcula
     * el %CPU promedio desde que se lanzo el proceso. */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    for (int i = 0; i < n; i++) {
        j->procs[i].pid = pids[i];
        j->procs[i].alive = 1;
        j->procs[i].prev_ticks = 0;
        j->procs[i].prev_time = now;
    }

    Job **pp = &jobs_head;          // se agrega al final para mantener el orden
    while (*pp) pp = &(*pp)->next;
    *pp = j;
    return j;
}

/*
 * Handler de SIGCHLD. Las senales estandar no se encolan: si 3 hijos mueren
 * casi juntos puede llegar UNA sola SIGCHLD, por eso se recolecta en ciclo
 * con WNOHANG hasta que no queden hijos terminados.
 * Solo usa waitpid() (async-signal-safe) y escribe campos sig_atomic_t;
 * no llama a malloc/free/printf.
 */
void jobs_handle_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (Job *j = jobs_head; j; j = j->next) {
            for (int i = 0; i < j->nprocs; i++) {
                if (j->procs[i].pid == pid && j->procs[i].alive) {
                    j->procs[i].alive = 0;
                    if (i == j->nprocs - 1) j->exit_status = status;
                    if (--j->remaining == 0) j->state = JOB_DONE;
                }
            }
        }
    }
    errno = saved_errno;
}

static void print_done_line(const Job *j) {
    int st = j->exit_status;
    if (WIFEXITED(st)) {
        int code = WEXITSTATUS(st);
        if (code == 0) printf("[%d]+ Done %s\n", j->id, j->cmdline);
        else           printf("[%d]+ Exit %d %s\n", j->id, code, j->cmdline);
    } else if (WIFSIGNALED(st)) {
        printf("[%d]+ Terminado (%s) %s\n", j->id, strsignal(WTERMSIG(st)), j->cmdline);
    } else {
        printf("[%d]+ Done %s\n", j->id, j->cmdline);
    }
}

/* Se llama antes de cada prompt: avisa y libera los jobs que terminaron. */
void jobs_notify_done(void) {
    sigset_t old;
    block_sigchld(&old);

    Job **pp = &jobs_head;
    while (*pp) {
        Job *j = *pp;
        if (j->state == JOB_DONE) {
            print_done_line(j);
            *pp = j->next;
            free(j);
        } else {
            pp = &j->next;
        }
    }
    fflush(stdout);
    restore_mask(&old);
}

/* Built-in "jobs" */
void jobs_print(void) {
    sigset_t old;
    block_sigchld(&old);

    Job **pp = &jobs_head;
    while (*pp) {
        Job *j = *pp;
        if (j->state == JOB_RUNNING) {
            printf("[%d] Ejecutando %s\n", j->id, j->cmdline);
            pp = &j->next;
        } else {
            print_done_line(j);
            *pp = j->next;
            free(j);
        }
    }
    restore_mask(&old);
}

int jobs_running(void) {
    int n = 0;
    for (Job *j = jobs_head; j; j = j->next)
        if (j->state == JOB_RUNNING) n++;
    return n;
}

/* Al salir de la shell: no dejar procesos huerfanos consumiendo CPU. */
void jobs_kill_all(void) {
    sigset_t old;
    block_sigchld(&old);

    Job *j = jobs_head;
    while (j) {
        Job *next = j->next;
        if (j->state == JOB_RUNNING) kill(-j->pgid, SIGTERM);
        free(j);
        j = next;
    }
    jobs_head = NULL;
    restore_mask(&old);
}
