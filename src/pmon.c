#include "pmon.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

// Banderas async-signal-safe
static volatile sig_atomic_t refresh_flag = 0;
static volatile sig_atomic_t stop_pmon = 0;

// Caché de estado anterior para aplicar el modelo diferencial de CPU
typedef struct {
    pid_t pid;
    unsigned long last_ticks;
    struct timespec last_time;
} ProcStateCache;

static ProcStateCache cache[MAX_JOBS];

// Manejadores asíncronos para interrupciones POSIX
static void sigalrm_handler(int sig) {
    (void)sig;
    refresh_flag = 1;
}

static void sigint_handler(int sig) {
    (void)sig;
    stop_pmon = 1;
}

// Lectura cruda de archivos del kernel sin llamadas a malloc
static int read_proc_file(const char *path, char *buf, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, size - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return 0;
}

void execute_pmon(int interval_sec) {
    long clk_tck = sysconf(_SC_CLK_TCK);
    char path[128];
    char stat_buf[1024];
    char status_buf[2048];

    // 1. Respaldo y sobreescritura de señales
    struct sigaction old_int, old_alrm, sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, &old_int);
    
    sa.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sa, &old_alrm);

    // Reiniciar banderas e inicializar caché local
    stop_pmon = 0;
    refresh_flag = 1; // Forzar primera iteración sin esperar
    memset(cache, 0, sizeof(cache));

    // Desactivar buffer de salida estándar para evitar tearing al pintar
    setvbuf(stdout, NULL, _IONBF, 0);

    // 2. Ciclo principal de hardware simulado
    while (!stop_pmon) {
        if (refresh_flag) {
            refresh_flag = 0;
            
            // Limpiar pantalla y posicionar el cursor arriba (Secuencia ANSI escape)
            printf("\033[H\033[J");
            printf("\033[1;36m%-6s | %-15s | %-5s | %-6s | %-8s\033[0m\n", 
                   "PID", "COMMAND", "STATE", "% CPU", "MEM(KiB)");
            printf("----------------------------------------------------------\n");

            struct timespec current_time;
            clock_gettime(CLOCK_MONOTONIC, &current_time);

            // Iteramos solo los descriptores de la tabla global de la shell
            
            for (int i = 0; i < MAX_JOBS; i++) {
                if (!jobs[i].active) continue;

                pid_t pid = jobs[i].pid;
                
                // --- Parseo Estricto de /proc/[pid]/stat ---
                snprintf(path, sizeof(path), "/proc/%d/stat", pid);
                if (read_proc_file(path, stat_buf, sizeof(stat_buf)) < 0) continue;

                // Optimización: El nombre del proceso puede tener espacios " (bash) ".
                // Buscamos el cierre de paréntesis desde la derecha.
                char *p = strrchr(stat_buf, ')');
                if (!p) continue;
                p += 2; // Avanzar al inicio de los campos numéricos (estado)

                char state;
                unsigned long utime, stime;
                // El kernel exporta un formato posicional estricto. Saltamos los campos
                // intermedios utilizando ignoradores de asignación "%*d" y "%*u"
               sscanf(p, "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &state, &utime, &stime);

                unsigned long current_ticks = utime + stime;
                double cpu_usage = 0.0;

                // --- Cálculo de % CPU ---
                if (cache[i].pid == pid) {
                    double delta_ticks = (double)(current_ticks - cache[i].last_ticks);
                    double delta_time = (current_time.tv_sec - cache[i].last_time.tv_sec) +
                                        (current_time.tv_nsec - cache[i].last_time.tv_nsec) / 1e9;
                    if (delta_time > 0.0) {
                        cpu_usage = (delta_ticks / (double)clk_tck / delta_time) * 100.0;
                    }
                }
                
                // Actualizar caché para la siguiente derivada temporal
                cache[i].pid = pid;
                cache[i].last_ticks = current_ticks;
                cache[i].last_time = current_time;

                // --- Parseo Lineal de /proc/[pid]/status ---
                snprintf(path, sizeof(path), "/proc/%d/status", pid);
                long mem_kb = 0;
                if (read_proc_file(path, status_buf, sizeof(status_buf)) == 0) {
                    // Cero llamadas a strtok_r: Búsqueda lineal directa de "VmRSS"
                    char *rss_ptr = strstr(status_buf, "VmRSS:");
                    if (rss_ptr) {
                        sscanf(rss_ptr + 6, "%ld", &mem_kb);
                    }
                }

                printf("%-6d | %-15.15s | %-5c | %-6.1f | %-8ld\n", 
                       pid, jobs[i].cmd_name, state, cpu_usage, mem_kb);
            }

            // Armar el timer del hardware simulado
            alarm(interval_sec);
        }

        // Suspender ejecución ahorrando ciclos de CPU hasta atrapar SIGALRM o SIGINT
        pause();
    }

    // 3. Limpieza final: Cancelar alarma residual y restaurar comportamiento original
    alarm(0);
    sigaction(SIGINT, &old_int, NULL);
    sigaction(SIGALRM, &old_alrm, NULL);
    
    // Devolver al REPL buffer de salida en modo normal
    setvbuf(stdout, NULL, _IOLBF, 0); 
    printf("\n"); // Salto de línea limpio tras presionar Ctrl+C
}