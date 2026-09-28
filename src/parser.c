#include "parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int parse_line(char *line, Pipeline *pipeline) {
    // Inicializar a cero toda la estructura
    memset(pipeline, 0, sizeof(Pipeline));
    
    // Eliminar salto de línea final
    line[strcspn(line, "\n")] = '\0';
    if (strlen(line) == 0) return 0;

    // Detección de background (&)
    size_t len = strlen(line);
while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t')) {
    line[--len] = '\0';
}

// Ahora sí detectamos si el último carácter válido es '&'
if (len > 0 && line[len - 1] == '&') {
    pipeline->is_background = 1;
    line[--len] = '\0'; // Eliminar el '&'
    
    // Volver a recortar por si había espacios antes del '&' (ej: "sleep 10   &")
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t')) {
        line[--len] = '\0';
    }
}

    char *saveptr_pipe;
    // 1. Separar por tuberías (|)
    char *cmd_token = strtok_r(line, "|", &saveptr_pipe);
    
    while (cmd_token != NULL && pipeline->count < MAX_CMDS) {
        Command *cmd = &pipeline->commands[pipeline->count];
        cmd->argc = 0;

        char *saveptr_space;
        // 2. Separar cada comando por espacios/tabulaciones
        char *arg_token = strtok_r(cmd_token, " \t", &saveptr_space);
        
        while (arg_token != NULL && cmd->argc < MAX_ARGS - 1) {
            if (strcmp(arg_token, "<") == 0) {
                // Redirección de entrada
                arg_token = strtok_r(NULL, " \t", &saveptr_space);
                if (arg_token) cmd->input_file = arg_token;
                
            } else if (strcmp(arg_token, ">") == 0) {
                // Redirección de salida (truncar)
                arg_token = strtok_r(NULL, " \t", &saveptr_space);
                if (arg_token) {
                    cmd->output_file = arg_token;
                    cmd->append_output = 0;
                }
                
            } else if (strcmp(arg_token, ">>") == 0) {
                // Redirección de salida (añadir)
                arg_token = strtok_r(NULL, " \t", &saveptr_space);
                if (arg_token) {
                    cmd->output_file = arg_token;
                    cmd->append_output = 1;
                }
                
            } else {
                // Argumento normal del binario
                cmd->argv[cmd->argc++] = arg_token;
            }
            
            // Avanzar al siguiente token
            arg_token = strtok_r(NULL, " \t", &saveptr_space);
        }
        
        // Cierre estricto del vector argv para execvp
        cmd->argv[cmd->argc] = NULL;
        
        // Registrar el comando solo si tiene argumentos o redirecciones
        if (cmd->argc > 0 || cmd->input_file || cmd->output_file) {
            pipeline->count++;
        }
        
        // Avanzar a la siguiente tubería
        cmd_token = strtok_r(NULL, "|", &saveptr_pipe);
    }

    return pipeline->count;
}

void free_pipeline(Pipeline *pipeline) {
    // Al mutar in-situ la memoria de la cadena original leída por getline,
    // no hay memoria dinámica independiente que liberar por comando.
    (void)pipeline; 
}