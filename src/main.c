#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "parser.h"

#define COLOR_CYAN "\033[1;36m"
#define COLOR_RESET "\033[0m"

void print_prompt(void) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf(COLOR_CYAN "%s" COLOR_RESET " $ ", cwd);
    } else {
        printf("mishell $ ");
    }
    fflush(stdout);
}

int main(void) {
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    Pipeline pipeline;

    while (1) {
        print_prompt();

        nread = getline(&line, &len, stdin);
        if (nread == -1) {
            // Manejo de Ctrl+D (EOF)
            printf("\nSaliendo de la shell...\n");
            break;
        }

        if (parse_line(line, &pipeline) > 0) {
            // Test temporal: Imprimir lo parseado
            printf("[DEBUG] Comando: %s | Args: %d | Background: %s\n",
                   pipeline.commands[0].argv[0],
                   pipeline.commands[0].argc,
                   pipeline.is_background ? "SI" : "NO");

            // TODO Fase 2: Built-ins y Executor
        }
    }

    free(line);
    return 0;
}
