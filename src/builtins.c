#include "builtins.h"
#include "jobs.h"
#include "pmon.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PMON_DEFAULT_INTERVAL 2

int is_builtin(const char *cmd) {
    if (cmd == NULL) return 0;
    return (strcmp(cmd, "cd") == 0 ||
            strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "jobs") == 0 ||
            strcmp(cmd, "pmon") == 0);
}

int execute_builtin(Command *cmd, int in_shell) {
    const char *name = cmd->argv[0];

    if (strcmp(name, "exit") == 0) {
        int code = 0;
        if (cmd->argv[1] != NULL) {
            char *end;
            errno = 0;
            long v = strtol(cmd->argv[1], &end, 10);
            if (errno != 0 || *end != '\0' || end == cmd->argv[1]) {
                fprintf(stderr, "exit: %s: se requiere un argumento numerico\n", cmd->argv[1]);
                return -1;
            }
            code = (int)v;
        }
        if (in_shell) {
            jobs_kill_all();   // no dejar procesos huerfanos
            exit(code);
        }
        fflush(stdout);
        _exit(code);           // dentro de un pipeline solo termina el hijo
    }

    if (strcmp(name, "cd") == 0) {
        const char *path = cmd->argv[1];
        if (path == NULL) {
            path = getenv("HOME");
            if (path == NULL) {
                fprintf(stderr, "cd: HOME no definido\n");
                return -1;
            }
        }
        if (chdir(path) != 0) {
            fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
            return -1;
        }
        return 0;
    }

    if (strcmp(name, "jobs") == 0) {
        jobs_print();
        return 0;
    }

    if (strcmp(name, "pmon") == 0) {
        int interval = PMON_DEFAULT_INTERVAL;
        if (cmd->argv[1] != NULL) {
            char *end;
            long v = strtol(cmd->argv[1], &end, 10);
            if (*end != '\0' || end == cmd->argv[1] || v <= 0 || v > 3600) {
                fprintf(stderr, "uso: pmon [segundos]   (entero entre 1 y 3600)\n");
                return -1;
            }
            interval = (int)v;
        }
        pmon_run(interval);
        return 0;
    }

    return 0;
}
