#include "parser.h"
#include <string.h>
#include <stdlib.h>

int parse_line(char *line, Pipeline *pipeline) {
    // Limpiar estructura previa
    memset(pipeline, 0, sizeof(Pipeline));
    
    // Eliminar salto de línea final
    line[strcspn(line, "\n")] = '\0';

    if (strlen(line) == 0) {
        return 0;
    }

    // Detección de background (&)
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '&') {
        pipeline->is_background = 1;
        line[len - 1] = '\0';
    }

    // Parseo simplificado de un solo comando (Fase 1)
    Command *cmd = &pipeline->commands[0];
    cmd->argc = 0;

    char *saveptr;
    char *token = strtok_r(line, " \t\r\n", &saveptr);
    while (token != NULL && cmd->argc < MAX_ARGS - 1) {
        cmd->argv[cmd->argc++] = token;
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }
    cmd->argv[cmd->argc] = NULL; // Obligatorio para execvp

    if (cmd->argc > 0) {
        pipeline->count = 1;
    }

    return pipeline->count;
}

void free_pipeline(Pipeline *pipeline) {
    // Si más adelante usas strdup() para los tokens, libera aquí la memoria.
    (void)pipeline;
}
