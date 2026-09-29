#include "executor.h"
#include "builtins.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

void execute_pipeline(Pipeline *pipeline) {
    if (pipeline->count == 0) return;

    // Si es un comando interno unico (cd o exit) lo ejecutamos en el padre
    if (pipeline->count == 1 && is_builtin(pipeline->commands[0].argv[0])) {
        execute_builtin(&pipeline->commands[0]);
        return;
    }

    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    int num_pipes = pipeline->count - 1;
    int pipefds[2 * MAX_CMDS];

    // Crear todos los pipes que se necesiten
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe");
            return;
        }
    }

    pid_t pids[MAX_CMDS];

    for (int i = 0; i < pipeline->count; i++) {
        Command *cmd = &pipeline->commands[i];
        
        // Bloqueamos SIGCHLD antes del fork para que no haya drama al guardar el job
        sigprocmask(SIG_BLOCK, &mask, &prev_mask);
        
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("fork");
            return;
        }

        if (pids[i] == 0) {
            // --- PROCESO HIJO ---
            
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);

            if (pipeline->is_background) {
                setpgid(0, 0); // Lo mandamos a su propio grupo para que ignore Ctrl+C
            } else {
                // En foreground, devolvemos las senales a la normalidad
                struct sigaction sa_dfl;
                sa_dfl.sa_handler = SIG_DFL;
                sigemptyset(&sa_dfl.sa_mask);
                sa_dfl.sa_flags = 0;
                sigaction(SIGINT, &sa_dfl, NULL);
                sigaction(SIGQUIT, &sa_dfl, NULL);
            }

            // Conectar pipes (si no es el primero ni el ultimo)
            if (i > 0) { 
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            if (i < pipeline->count - 1) { 
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }

            // Redirecciones con archivos (<, >, >>)
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

            // Cerrar todos los pipes en el hijo, super importante
            for (int j = 0; j < 2 * num_pipes; j++) {
                close(pipefds[j]);
            }

            execvp(cmd->argv[0], cmd->argv);
            perror("execvp");
            _exit(EXIT_FAILURE);
            
        } else {
            // --- PROCESO PADRE (LA SHELL) ---
            
            // Registramos el job cuando es background (usamos el ultimo comando del pipe)
            if (pipeline->is_background && i == pipeline->count - 1) {
                char full_cmd[256] = "";
                
                // Reconstruir todo el comando con argumentos y pipes para mostrarlo bien
                for (int c = 0; c < pipeline->count; c++) {
                    Command *bcmd = &pipeline->commands[c];
                    for (int k = 0; k < bcmd->argc; k++) {
                        strncat(full_cmd, bcmd->argv[k], sizeof(full_cmd) - strlen(full_cmd) - 1);
                        if (k < bcmd->argc - 1) {
                            strncat(full_cmd, " ", sizeof(full_cmd) - strlen(full_cmd) - 1);
                        }
                    }
                    if (c < pipeline->count - 1) {
                        strncat(full_cmd, " | ", sizeof(full_cmd) - strlen(full_cmd) - 1);
                    }
                }

                int jid = add_job(pids[i], full_cmd);
                printf("[%d] %d\n", jid, pids[i]);
            }
            
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        }
    }

    // Cerrar los pipes en el padre para no quedarse pegado leyendo
    for (int i = 0; i < 2 * num_pipes; i++) {
        close(pipefds[i]);
    }

    // Esperar a que todo termine si esta en foreground
    if (!pipeline->is_background) {
        for (int i = 0; i < pipeline->count; i++) {
            int status;
            waitpid(pids[i], &status, 0); 
        }
    }
}