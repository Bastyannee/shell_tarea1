#ifndef PMON_H
#define PMON_H

#include <sys/types.h>
#include <time.h>

/**
 * @brief Inicia el monitor interactivo de procesos (similar a top).
 * 
 * @param interval_sec Intervalo de refresco en segundos.
 */
void execute_pmon(int interval_sec);

#endif /* PMON_H */