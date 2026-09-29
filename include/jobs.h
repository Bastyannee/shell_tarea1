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

/**
 * @brief Inicializa la estructura global de trabajos (jobs).
 */
void init_jobs(void);

/**
 * @brief Agrega un nuevo proceso a la tabla de background.
 * 
 * @param pid ID del proceso hijo.
 * @param cmd_name Línea de comando original (ej. "sleep 30").
 * @return int ID del job asignado, o -1 si la tabla está llena.
 */
int add_job(pid_t pid, const char *cmd_name);

/**
 * @brief Manejador asíncrono para recolectar hijos terminados y evitar zombies.
 * 
 * @param sig Número de la señal (SIGCHLD).
 */
void sigchld_handler(int sig);

/**
 * @brief Revisa la tabla e imprime notificaciones de trabajos terminados.
 */
void check_completed_jobs(void);

/**
 * @brief Lista todos los procesos activos en segundo plano.
 */
void list_jobs(void);

#endif /* JOBS_H */