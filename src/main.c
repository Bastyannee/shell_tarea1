#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

/*
 * R6: la shell ignora SIGINT/SIGQUIT (Ctrl+C / Ctrl+\ en el prompt no la matan)
 * y recolecta hijos de background con un handler de SIGCHLD.
 */
static void setup_shell_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    /* Ctrl+Z no esta implementado (bonus): se ignora para no dejar la shell
     * detenida; los hijos heredan esta disposicion. */
    sigaction(SIGTSTP, &sa, NULL);

    sa.sa_handler = jobs_handle_sigchld;
    sa.sa_flags = SA_RESTART;   // getline()/read() se reanudan tras el handler
    sigaction(SIGCHLD, &sa, NULL);
}

int main(void) {
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    Pipeline pipeline;

    setup_shell_signals();

    while (1) {
        jobs_notify_done();   // "[1]+ Done ..." antes de mostrar el prompt
        print_prompt();

        nread = getline(&line, &len, stdin);
        if (nread == -1) {
            // Manejo de Ctrl+D (EOF)
            printf("\nSaliendo de la shell...\n");
            break;
        }

        if (parse_line(line, &pipeline) > 0) {
            if (execute_pipeline(&pipeline) < 0) {
                fprintf(stderr, "mishell: error fatal de fork(), cerrando la shell\n");
                break;
            }
        }
        free_pipeline(&pipeline);
    }

    jobs_kill_all();
    free(line);
    return 0;
}
