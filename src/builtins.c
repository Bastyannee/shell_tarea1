#include "builtins.h"
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

    // "jobs" y "pmon" se integrarán en las siguientes fases
    return 0;
}
