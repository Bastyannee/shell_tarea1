#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

// Devuelve 0 en operación normal y -1 si ocurrió un error fatal de fork().
int execute_pipeline(Pipeline *pipeline);

#endif
