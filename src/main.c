#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "parser.h"
#include "executor.h"
#include "jobs.h"

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

    // 1. Inicializar sistema de Jobs
    init_jobs();

    // 2. Configurar señales para que la Shell no muera accidentalmente
    struct sigaction sa_ign, sa_chld;
    
    // Ignorar SIGINT y SIGQUIT
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    sa_ign.sa_flags = 0;
    sigaction(SIGINT, &sa_ign, NULL);
    sigaction(SIGQUIT, &sa_ign, NULL);

    // Asignar el manejador de SIGCHLD
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP; 
    sigaction(SIGCHLD, &sa_chld, NULL);

    while (1) {
        // 3. Revisar y notificar trabajos finalizados
        check_completed_jobs();

        print_prompt();

        nread = getline(&line, &len, stdin);
        if (nread == -1) {
            printf("\n");
            break;
        }

        if (parse_line(line, &pipeline) > 0) {
            execute_pipeline(&pipeline);
        }
    }

    free(line);
    return 0;
}