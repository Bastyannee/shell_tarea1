#include "executor.h"
#include "builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

void execute_pipeline(Pipeline *pipeline) {
    if (pipeline->count == 0) return;

    // Caso 1: Comando único y es built-in
    if (pipeline->count == 1 && is_builtin(pipeline->commands[0].argv[0])) {
        execute_builtin(&pipeline->commands[0]);
        return;
    }

    // Caso 2: Comando externo simple (Fase 2)
    if (pipeline->count == 1) {
        Command *cmd = &pipeline->commands[0];
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            return;
        }

        if (pid == 0) {
            // Proceso Hijo:
            // Reemplaza el espacio de direcciones con el ejecutable buscado en $PATH
            if (execvp(cmd->argv[0], cmd->argv) == -1) {
                perror("execvp");
                _exit(EXIT_FAILURE); // Se usa _exit() para no vaciar buffers del padre
            }
        } else {
            // Proceso Padre (Shell):
            if (!pipeline->is_background) {
                int status;
                // Espera bloqueante hasta que el hijo termine
                waitpid(pid, &status, 0);
            } else {
                printf("[Background PID %d]\n", pid);
            }
        }
        return;
    }

    // TODO Fase 3: Encadenamiento de pipelines (|)
}
