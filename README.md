# mishell - Shell simple en C (Tarea 1, Sistemas Operativos 2026)

Shell de texto para Linux con pipes, redirecciones, ejecución en background,
manejo de señales y el monitor `pmon`.

## Compilar

```bash
make clean && make      # genera el ejecutable ./mishell
```

Compila sin advertencias con `gcc -Wall -Wextra -std=gnu11`.

## Ejecutar

```bash
./mishell
```

Salir con `exit [n]` o Ctrl+D.

## Funcionalidades

| Requisito | Descripción |
|-----------|-------------|
| R1 | Prompt con directorio actual, `getline()`, tokenizador propio (comillas, espacios múltiples, operadores sin espacios) |
| R2 | Built-ins: `cd [dir]`, `exit [n]`, `jobs`, `pmon [segundos]` |
| R3 | Redirección `<`, `>`, `>>` con `open()` + `dup2()` + `close()` |
| R4 | Pipes de largo arbitrario (hasta 16 comandos): `cmd1 \| cmd2 \| ... \| cmdN` |
| R5 | Background con `&`, lista de jobs, `SIGCHLD` con `waitpid(-1, WNOHANG)` en ciclo, aviso `[1]+ Done ...` |
| R6 | La shell ignora `SIGINT`/`SIGQUIT`; los hijos restauran `SIG_DFL`; los jobs de background van en su propio grupo de procesos |
| pmon | Tabla de jobs con estado, %CPU y RSS leyendo `/proc/[pid]/stat` y `/proc/[pid]/status`; refresco con `alarm()`/`SIGALRM`; termina con Ctrl+C |
| Bonus | En `pmon` la tabla se ordena por %CPU y se resalta el mayor consumidor |

## Ejemplos

```
$ ls -l | grep ".c" | wc -l
$ sort < datos.txt > ordenados.txt
$ sleep 30 &
[1] 4821
$ yes > /dev/null &
[2] 4830
$ jobs
[1] Ejecutando sleep 30
[2] Ejecutando yes
$ pmon 2
```

## Estructura

```
include/  parser.h executor.h builtins.h jobs.h pmon.h
src/
  main.c       REPL, señales de la shell, aviso de jobs terminados
  parser.c     tokenización -> estructuras Command / Pipeline
  executor.c   fork/exec, pipes de N comandos, redirecciones, foreground/background
  builtins.c   cd, exit, jobs, pmon
  jobs.c       lista enlazada de jobs y handler de SIGCHLD
  pmon.c       lectura de /proc, cálculo de %CPU, refresco con SIGALRM
```

## Limitaciones conocidas

- No hay `;`, `&&`, `||`, globbing ni expansión de variables.
- Solo se admite un `&` y debe ir al final de la línea.
- Ctrl+Z (SIGTSTP) se ignora; `fg`/`bg` no están implementados.
- Al salir (`exit` o Ctrl+D) la shell envía SIGTERM a los jobs que sigan vivos.
