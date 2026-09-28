#ifndef PMON_H
#define PMON_H

// Monitor de jobs en background. Refresca cada "interval" segundos hasta Ctrl+C.
void pmon_run(int interval);

#endif
