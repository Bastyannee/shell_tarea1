#include "builtins.h"
#include "jobs.h"  
#include "pmon.h"  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Verifica si el comando ingresado es uno de los internos de nuestra shell
int is_builtin(const char *cmd) {
    if (cmd == NULL) return 0;
    return (strcmp(cmd, "cd") == 0 ||
            strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "jobs") == 0 ||
            strcmp(cmd, "pmon") == 0);
}

// Ejecuta el comando interno directamente en el proceso padre (no hace fork)
int execute_builtin(Command *cmd) {
    
    // Implementacion de 'exit [n]'
    if (strcmp(cmd->argv[0], "exit") == 0) {
        int status = 0;
        // Si el usuario especifica un codigo de salida, lo usamos (ej: exit 5)
        if (cmd->argv[1] != NULL) {
            status = atoi(cmd->argv[1]);
        }
        exit(status);
    }

    // Implementacion de 'cd [dir]'
    if (strcmp(cmd->argv[0], "cd") == 0) {
        char *path = cmd->argv[1];
        // Si no le pasan argumentos, nos vamos al $HOME del sistema
        if (path == NULL) {
            path = getenv("HOME");
            if (path == NULL) {
                fprintf(stderr, "cd: la variable HOME no esta definida\n");
                return -1;
            }
        }
        // chdir cambia el directorio en el proceso actual de la shell
        if (chdir(path) != 0) {
            perror("cd"); // Muestra por que fallo 
            return -1;
        }
        return 0;
    }

    // Implementacion de 'jobs'
    if (strcmp(cmd->argv[0], "jobs") == 0) {
        list_jobs(); // Llama a la funcion de jobs.c que imprime la tabla de bg
        return 0;
    }

    // Implementacion de 'pmon [segundos]'
    if (strcmp(cmd->argv[0], "pmon") == 0) {
        int interval = 2; // Refresco default de 2 segundos segun las instrucciones
        
        // Si le pasan un argumento de tiempo, actualizamos el intervalo
        if (cmd->argv[1] != NULL) {
            interval = atoi(cmd->argv[1]);
            // Si el usuario mete un texto o un numero negativo, forzamos a 2s
            if (interval <= 0) interval = 2; 
        }
        execute_pmon(interval); // Lanza el loop del monitor de procesos
        return 0;
    }

    return 0;
}