#include "builtins.h"
#include "jobs.h"  // Necesario para list_jobs()
#include "pmon.h"  // Necesario para execute_pmon()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int is_builtin(const char *cmd) {
    if (cmd == NULL) return 0;
    return (strcmp(cmd, "cd") == 0 ||
            strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "jobs") == 0 ||
            strcmp(cmd, "pmon") == 0);
}

int execute_builtin(Command *cmd) {
    if (strcmp(cmd->argv[0], "exit") == 0) {
        exit(0);
    }

    if (strcmp(cmd->argv[0], "cd") == 0) {
        char *path = cmd->argv[1];
        if (path == NULL) {
            path = getenv("HOME");
            if (path == NULL) {
                fprintf(stderr, "cd: HOME no definido\n");
                return -1;
            }
        }
        if (chdir(path) != 0) {
            perror("cd");
            return -1;
        }
        return 0;
    }

    if (strcmp(cmd->argv[0], "jobs") == 0) {
        list_jobs();
        return 0;
    }

    if (strcmp(cmd->argv[0], "pmon") == 0) {
        int interval = 2; // Default 2 segundos
        if (cmd->argv[1] != NULL) {
            interval = atoi(cmd->argv[1]);
            if (interval <= 0) interval = 2;
        }
        execute_pmon(interval);
        return 0;
    }

    return 0;
}