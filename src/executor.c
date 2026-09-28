#include "executor.h"
#include "builtins.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

void execute_pipeline(Pipeline *pipeline) {
    if (pipeline->count == 0) return;

    // Caso 1: Built-in único en el proceso padre
    if (pipeline->count == 1 && is_builtin(pipeline->commands[0].argv[0])) {
        execute_builtin(&pipeline->commands[0]);
        return;
    }

    // Preparar máscara para bloquear SIGCHLD durante la creación del job
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    int num_pipes = pipeline->count - 1;
    int pipefds[2 * MAX_CMDS];

    // Instanciar todas las tuberías necesarias
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe");
            return;
        }
    }

    pid_t pids[MAX_CMDS];

    for (int i = 0; i < pipeline->count; i++) {
        Command *cmd = &pipeline->commands[i];
        
        // Bloquear SIGCHLD ANTES del fork para evitar condiciones de carrera
        sigprocmask(SIG_BLOCK, &mask, &prev_mask);
        
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("fork");
            return;
        }

        if (pids[i] == 0) {
            // =========================
            // ESPACIO DEL HIJO
            // =========================
            
            // 1. Restaurar máscara de señales
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);

            // 2. Aislamiento de Señales
            if (pipeline->is_background) {
                setpgid(0, 0);
            } else {
                // Si es foreground, DEBE morir con Ctrl+C o Ctrl+Quit
                struct sigaction sa_dfl;
                sa_dfl.sa_handler = SIG_DFL;
                sigemptyset(&sa_dfl.sa_mask);
                sa_dfl.sa_flags = 0;
                sigaction(SIGINT, &sa_dfl, NULL);
                sigaction(SIGQUIT, &sa_dfl, NULL);
            }

            // 3. Conectar tuberías (Pipes)
            if (i > 0) { 
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            if (i < pipeline->count - 1) { 
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }

            // 4. Conectar Redirecciones Explícitas (<, >, >>)
            if (cmd->input_file != NULL) {
                int fd_in = open(cmd->input_file, O_RDONLY);
                if (fd_in < 0) {
                    perror("open input");
                    _exit(EXIT_FAILURE);
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            if (cmd->output_file != NULL) {
                int flags = O_WRONLY | O_CREAT;
                flags |= (cmd->append_output) ? O_APPEND : O_TRUNC;
                int fd_out = open(cmd->output_file, flags, 0644);
                if (fd_out < 0) {
                    perror("open output");
                    _exit(EXIT_FAILURE);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            // 5. Cerrar TODOS los descriptores de pipes en el hijo
            for (int j = 0; j < 2 * num_pipes; j++) {
                close(pipefds[j]);
            }

            // 6. Ejecutar binario
            execvp(cmd->argv[0], cmd->argv);
            perror("execvp");
            _exit(EXIT_FAILURE);
            
        } else {
            // =========================
            // ESPACIO DEL PADRE (SHELL)
            // =========================
            
            // Registrar solo el comando líder del pipeline si es background
            if (pipeline->is_background && i == pipeline->count - 1) {
                int jid = add_job(pids[i], pipeline->commands[0].argv[0]);
                printf("[%d] %d\n", jid, pids[i]);
            }
            // Desbloquear SIGCHLD tras registrar con éxito
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        }
    }

    // REGLA CRÍTICA: Cerrar TODOS los descriptores de pipes en la Shell padre
    for (int i = 0; i < 2 * num_pipes; i++) {
        close(pipefds[i]);
    }

    // Sincronización
    if (!pipeline->is_background) {
        for (int i = 0; i < pipeline->count; i++) {
            int status;
            waitpid(pids[i], &status, 0); 
        }
    }
}