#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>

#define MAX_ARGS 64
#define MAX_CMDS 16

typedef struct {
    char *argv[MAX_ARGS];  // Argumentos terminados en NULL para execvp
    int argc;
    char *input_file;      // Nombre del archivo si hay '<', o NULL
    char *output_file;     // Nombre del archivo si hay '>' o '>>', o NULL
    int append_output;     // 1 si es '>>', 0 si es '>'
} Command;

typedef struct {
    Command commands[MAX_CMDS]; // Lista de comandos encadenados por '|'
    int count;                  // Cantidad de comandos en el pipeline
    int is_background;          // 1 si la línea termina en '&', 0 si no
} Pipeline;

// Prototipos del módulo parser
int parse_line(char *line, Pipeline *pipeline);
void free_pipeline(Pipeline *pipeline);

#endif
