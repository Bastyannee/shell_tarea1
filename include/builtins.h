#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

/**
 * @brief Verifica si un comando pertenece a los comandos internos de la shell.
 * 
 * @param cmd Nombre del comando a verificar.
 * @return int 1 si es built-in (cd, exit, jobs, pmon), 0 en caso contrario.
 */
int is_builtin(const char *cmd);

/**
 * @brief Ejecuta la lógica de un comando interno en el proceso padre.
 * 
 * @param cmd Estructura Command que contiene los argumentos.
 * @return int 0 en caso de éxito, -1 en caso de error.
 */
int execute_builtin(Command *cmd);

#endif /* BUILTINS_H */