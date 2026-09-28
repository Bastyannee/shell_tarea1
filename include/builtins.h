#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

int is_builtin(const char *cmd);

// in_shell = 1: se ejecuta en el proceso de la shell.
// in_shell = 0: se ejecuta dentro de un hijo de un pipeline (ej: "jobs | grep x").
int execute_builtin(Command *cmd, int in_shell);

#endif
