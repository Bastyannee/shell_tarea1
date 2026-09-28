#include "pmon.h"
#include "jobs.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Banderas que solo escriben los handlers (regla: handlers minimos) */
static volatile sig_atomic_t alarm_flag = 0;
static volatile sig_atomic_t stop_flag = 0;

static void handle_alarm(int sig) { (void)sig; alarm_flag = 1; }
static void handle_int(int sig)   { (void)sig; stop_flag = 1; }

typedef struct {
    pid_t pid;
    char cmd[128];
    char state;
    double cpu;
    unsigned long rss_kb;
} Row;

/*
 * /proc/[pid]/stat: "pid (comm) S ppid pgrp ... utime stime ..."
 * comm puede tener espacios y parentesis, por eso se busca el ULTIMO ')'.
 * Campo 3 = state, campo 14 = utime, campo 15 = stime (en ticks).
 */
static int read_stat(pid_t pid, char *state, unsigned long *ticks, char *comm, size_t commsz) {
    char path[64], buf[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);

    FILE *f = fopen(path, "r");
    if (!f) return -1;                 // el proceso ya no existe
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return -1;
    buf[n] = '\0';

    char *lp = strchr(buf, '(');
    char *rp = strrchr(buf, ')');
    if (!lp || !rp || rp < lp) return -1;

    size_t clen = (size_t)(rp - lp - 1);
    if (clen >= commsz) clen = commsz - 1;
    memcpy(comm, lp + 1, clen);
    comm[clen] = '\0';

    unsigned long ut, st;
    /* despues de ')': state, luego se saltan los campos 4..13 y se leen 14 y 15 */
    if (sscanf(rp + 1, " %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
               state, &ut, &st) != 3)
        return -1;

    *ticks = ut + st;
    return 0;
}

/* /proc/[pid]/status -> linea "VmRSS:   412 kB". Los zombies no la tienen (0). */
static unsigned long read_rss_kb(pid_t pid) {
    char path[64], line[256];
    unsigned long kb = 0;
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);

    FILE *f = fopen(path, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%lu", &kb);
            break;
        }
    }
    fclose(f);
    return kb;
}

/* /proc/[pid]/cmdline: argumentos separados por '\0'. Si esta vacio, usa comm. */
static void read_cmdline(pid_t pid, const char *comm, char *out, size_t sz) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);

    size_t n = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        n = fread(out, 1, sz - 1, f);
        fclose(f);
    }
    if (n == 0) {
        snprintf(out, sz, "%s", comm);
        return;
    }
    for (size_t i = 0; i + 1 < n; i++)
        if (out[i] == '\0') out[i] = ' ';
    out[n] = '\0';
    while (n > 0 && out[n - 1] == ' ') out[--n] = '\0';
}

static const char *state_name(char s) {
    switch (s) {
    case 'R': return "ejecutando";
    case 'S': return "durmiendo";
    case 'D': return "espera-E/S";
    case 'Z': return "zombie";
    case 'T': return "detenido";
    default:  return "otro";
    }
}

static int cmp_rows(const void *a, const void *b) {
    const Row *ra = a, *rb = b;
    if (ra->cpu < rb->cpu) return 1;      // mayor %CPU primero
    if (ra->cpu > rb->cpu) return -1;
    return (ra->pid > rb->pid) - (ra->pid < rb->pid);
}

static void draw(int interval) {
    long hz = sysconf(_SC_CLK_TCK);
    if (hz <= 0) hz = 100;

    /* cantidad maxima de filas (los procesos solo pueden disminuir) */
    int cap = 0;
    for (Job *j = jobs_first(); j; j = j->next)
        if (j->state == JOB_RUNNING)
            for (int i = 0; i < j->nprocs; i++)
                if (j->procs[i].alive) cap++;

    Row *rows = malloc((size_t)(cap > 0 ? cap : 1) * sizeof(Row));
    int nrows = 0;
    if (!rows) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    for (Job *j = jobs_first(); j; j = j->next) {
        if (j->state != JOB_RUNNING) continue;
        for (int i = 0; i < j->nprocs && nrows < cap; i++) {
            JobProc *p = &j->procs[i];
            if (!p->alive) continue;

            char state, comm[64];
            unsigned long ticks;
            /* Si el proceso termino entre la lectura de la lista y este open(),
             * /proc/[pid] ya no existe: se omite y la proxima vez ya no esta. */
            if (read_stat(p->pid, &state, &ticks, comm, sizeof(comm)) != 0) continue;

            double dt = (double)(now.tv_sec - p->prev_time.tv_sec) +
                        (double)(now.tv_nsec - p->prev_time.tv_nsec) / 1e9;
            double cpu = 0.0;
            if (dt > 0.0 && ticks >= p->prev_ticks)
                cpu = ((double)(ticks - p->prev_ticks) / (double)hz) / dt * 100.0;
            p->prev_ticks = ticks;
            p->prev_time = now;

            Row *r = &rows[nrows++];
            r->pid = p->pid;
            r->state = state;
            r->cpu = cpu;
            r->rss_kb = read_rss_kb(p->pid);
            read_cmdline(p->pid, comm, r->cmd, sizeof(r->cmd));
        }
    }

    qsort(rows, (size_t)nrows, sizeof(Row), cmp_rows);

    printf("\033[H\033[J");   // limpiar pantalla
    printf("pmon - refresco cada %d s - Ctrl+C para salir\n\n", interval);
    printf("%-8s %-32s %-12s %11s %9s\n", "PID", "COMANDO", "ESTADO", "%CPU(aprox)", "RSS(KB)");
    for (int i = 0; i < nrows; i++) {
        int hot = (i == 0 && rows[i].cpu > 0.0);   // resalta el mayor consumidor
        if (hot) printf("\033[1;31m");
        printf("%-8d %-32.32s %-12s %11.1f %9lu\n", (int)rows[i].pid, rows[i].cmd,
               state_name(rows[i].state), rows[i].cpu, rows[i].rss_kb);
        if (hot) printf("\033[0m");
    }
    if (nrows == 0) printf("(no quedan jobs en ejecucion)\n");
    fflush(stdout);
    free(rows);
}

void pmon_run(int interval) {
    if (jobs_running() == 0) {
        printf("pmon: no hay jobs en background\n");
        return;
    }

    /* Handlers temporales (sigaction, sin SA_RESTART para que interrumpan la espera) */
    struct sigaction sa_alrm, sa_int, old_alrm, old_int;
    memset(&sa_alrm, 0, sizeof(sa_alrm));
    memset(&sa_int, 0, sizeof(sa_int));
    sa_alrm.sa_handler = handle_alarm;
    sa_int.sa_handler = handle_int;
    sigemptyset(&sa_alrm.sa_mask);
    sigemptyset(&sa_int.sa_mask);

    alarm_flag = 0;
    stop_flag = 0;
    sigaction(SIGALRM, &sa_alrm, &old_alrm);
    sigaction(SIGINT, &sa_int, &old_int);

    /* Se bloquean SIGALRM/SIGINT y se espera con sigsuspend(): desbloquea y
     * duerme de forma atomica, evitando la carrera de "revisar bandera y luego
     * pause()" (una senal entre ambos pasos dejaria a pmon dormido para siempre). */
    sigset_t block, oldmask;
    sigemptyset(&block);
    sigaddset(&block, SIGALRM);
    sigaddset(&block, SIGINT);
    sigprocmask(SIG_BLOCK, &block, &oldmask);

    draw(interval);
    alarm((unsigned int)interval);

    while (!stop_flag) {
        while (!alarm_flag && !stop_flag)
            sigsuspend(&oldmask);          // tambien despierta con SIGCHLD: se ignora y se sigue
        if (stop_flag) break;

        alarm_flag = 0;
        draw(interval);
        alarm((unsigned int)interval);
    }

    alarm(0);                              // cancelar alarma pendiente
    sigaction(SIGALRM, &old_alrm, NULL);   // restaurar disposiciones de la shell
    sigaction(SIGINT, &old_int, NULL);
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    printf("\n");
}
