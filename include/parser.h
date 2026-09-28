#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>

#define MAX_ARGS 64
#define MAX_CMDS 16

typedef struct {
    char *argv[MAX_ARGS];  // Argumentos terminados en NULL para execvp
    int argc;              // Cantidad de argumentos en argv
    char *input_file;      // Descriptor origen si existe '<', o NULL
    char *output_file;     // Descriptor destino si existe '>' o '>>', o NULL
    int append_output;     // 1 si la redirección es '>>' (O_APPEND), 0 si es '>' (O_TRUNC)
} Command;

typedef struct {
    Command commands[MAX_CMDS]; // Arreglo contiguo de N comandos encadenados por '|'
    int count;                  // Cantidad total de comandos en la tubería (N <= MAX_CMDS)
    int is_background;          // Bandera booleana: 1 si finaliza en '&', 0 en caso contrario
} Pipeline;

/**
 * @brief Parsea una línea de comando mutando el buffer original in situ.
 * Separa por pipelines '|', identifica redirecciones '<', '>', '>>' y background '&'.
 * 
 * @param line Cadena leída por getline() terminada en '\0'.
 * @param pipeline Puntero a la estructura Pipeline a poblar.
 * @return int Cantidad de comandos parseados en el pipeline (0 si está vacía).
 */
int parse_line(char *line, Pipeline *pipeline);

/**
 * @brief Libera los recursos dinámicos asociados al pipeline si los hubiera.
 */
void free_pipeline(Pipeline *pipeline);

#endif /* PARSER_H */