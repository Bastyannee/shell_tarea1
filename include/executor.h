#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

/**
 * @brief Orquesta la ejecución de un pipeline de comandos.
 * Crea procesos hijos, conecta tuberías y maneja redirecciones de E/S.
 * 
 * @param pipeline Puntero a la estructura con los comandos parseados.
 */
void execute_pipeline(Pipeline *pipeline);

#endif /* EXECUTOR_H */