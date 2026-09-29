#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 64
#define MAX_CMDS 16

typedef struct {
    char *argv[MAX_ARGS];  
    int argc;              
    char *input_file;      
    char *output_file;     
    int append_output;     
} Command;

typedef struct {
    Command commands[MAX_CMDS]; 
    int count;                  
    int is_background;          
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
 * 
 * @param pipeline Puntero a la estructura Pipeline.
 */
void free_pipeline(Pipeline *pipeline);

#endif /* PARSER_H */