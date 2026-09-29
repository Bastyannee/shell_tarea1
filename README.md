# mishell - Intérprete de Comandos en Espacio de Usuario

Implementación de una shell interactiva simple en lenguaje C sobre arquitectura POSIX/Linux, orientada a la administración de procesos, manipulación de descriptores de archivos, encadenamiento de tuberías, recolección asíncrona de señales e inspección de rendimiento del kernel mediante el sistema de archivos virtual `/proc`.

Desarrollado para la asignatura **Sistemas Operativos (501251)**, Universidad de Concepción.

---

## 1. Características Técnicas

* **Comandos Internos (*Built-ins*):**
  * `cd [directorio]`: Modifica el directorio de trabajo actual (*cwd*) del proceso padre mediante `chdir()`. Si no recibe argumentos, navega al directorio definido en `$HOME`.
  * `exit`: Termina de forma limpia la sesión de la shell.
  * `jobs`: Lista todos los procesos activos o en segundo plano en ejecución o detenidos (`[job_id] PID Estado Comando`).
  * `pmon [segundos]`: Monitor de procesos en tiempo real con refresco configurable (por defecto 2s). Extrae métricas de `/proc/[pid]/stat` y `/proc/[pid]/status` sin asignación dinámica de memoria (`malloc`), calculando el uso porcentual de CPU mediante muestras diferenciales de ticks normalizados con `_SC_CLK_TCK`.
* **Ejecución y Ciclo de Vida de Procesos:**
  * Ejecución de binarios externos mediante el patrón canónico `fork()` y `execvp()`.
  * Resolución de rutas utilizando la variable de entorno `$PATH`.
* **Redirección de Entrada y Salida ($E/S$):**
  * `<` : Redirección de entrada estándar (`STDIN_FILENO`) con flags `O_RDONLY`.
  * `>` : Redirección de salida estándar (`STDOUT_FILENO`) en modo truncado (`O_WRONLY | O_CREAT | O_TRUNC`).
  * `>>`: Redirección de salida estándar en modo adición (`O_WRONLY | O_CREAT | O_APPEND`).
* **Tuberías Arbitrarias (Pipelines):**
  * Soporte para $N$ comandos encadenados mediante $N-1$ tuberías (`pipe()`).
  * Conexión de flujos mediante duplicación de descriptores con `dup2()`.
  * Cierre preventivo y exhaustivo de todos los extremos de lectura y escritura en los procesos hijos y en la shell padre, previniendo *deadlocks* por retención de EOF.
* **Procesos en Segundo Plano y Control Asíncrono:**
  * Operador `&` al final de la línea para ejecución asíncrona en *background*.
  * Aislamiento de procesos secundarios en grupos independientes mediante `setpgid(0, 0)` para inmunizarlos ante interrupciones de teclado (`Ctrl+C`).
  * Manejador asíncrono *signal-safe* para `SIGCHLD` que vacía procesos zombie de forma no bloqueante con `waitpid(-1, &status, WNOHANG | WUNTRACED)`.
  * Preservación del estado de `errno` en manejadores y protección de secciones críticas mediante enmascaramiento con `sigprocmask()`.

---

## 2. Estructura del Repositorio

El diseño desacopla responsabilidades en módulos independientes compilados de forma separada:

```text
.
├── Makefile              # Reglas de compilación automatizada y limpieza
├── README.md             # Documentación general del proyecto
├── include/              # Interfaces públicas (headers)
│   ├── builtins.h        # Definición de comandos internos (cd, exit, jobs, pmon)
│   ├── executor.h        # Motor de pipelines, fork(), execvp() y descriptores
│   ├── jobs.h            # Tabla de trabajos y manejadores de señales POSIX
│   ├── parser.h          # Estructuras de datos (Command, Pipeline) y tokenizado
│   └── pmon.h            # Interfaz del monitor de procesos sobre /proc
└── src/                  # Implementación modular en C
    ├── builtins.c        # Lógica de los built-ins de la shell
    ├── executor.c        # Gestión de procesos, tuberías y redirecciones
    ├── jobs.c            # Manejo de tabla estática y recolección con SIGCHLD
    ├── main.c            # Ciclo REPL, configuración inicial de señales y prompt
    ├── parser.c          # Tokenizador in-situ y análisis sintáctico
    └── pmon.c            # Lógica de /proc, timer con SIGALRM y renderizado

```

---

## 3. Requisitos y Compilación

* **Compilador:** GCC compatible con el estándar C11 (`gnu11`).
* **Sistema Operativo:** Distribución Linux con kernel compatible con la interfaz `/proc`.
* **Herramientas:** GNU Make.

Para compilar el proyecto con todas las banderas de advertencia habilitadas (`-Wall -Wextra -std=gnu11`):

```bash
make clean && make

```

Esto generará el binario ejecutable `mishell` en la raíz del repositorio.

Para remover los archivos objeto intermediarios (`*.o`) y el ejecutable:

```bash
make clean

```

---

## 4. Guía de Uso y Ejemplos

Inicie la shell ejecutando:

```bash
./mishell

```

### Comandos Básicos y Redirecciones

```bash
# Navegación entre directorios
cd /usr/include
pwd
cd ..

# Redirección de salida a archivo (truncar y añadir)
echo "Sistemas Operativos 2026" > archivo.txt
echo "Universidad de Concepcion" >> archivo.txt

# Redirección de entrada
cat < archivo.txt

```

### Tuberías de Múltiples Fases ($N \ge 3$)

```bash
# Encadenamiento de tres o más programas
cat /etc/passwd | cut -d: -f1 | sort | tr 'a-z' 'A-Z'

# Combinación simultánea de tuberías y redirección a archivo
cat < Makefile | grep TARGET > binarios.txt

```

### Trabajos en Segundo Plano (*Background*) y `jobs`

```bash
# Ejecutar un proceso prolongado en background
sleep 20 &

# Consultar el estado de los trabajos registrados
jobs

# Notificación automática de término (al presionar Enter una vez concluido)
# [1]+ Done        sleep

```

### Monitor de Rendimiento `pmon`

```bash
# Ejecutar una tarea en background para monitorear
sleep 50 &

# Iniciar pmon con refresco cada 1 segundo
pmon 1

```

*Para salir de `pmon` y regresar al intérprete de comandos interactivo, presione `Ctrl+C`.*

---

## 5. Autores

* Bastian Pérez Aguayo
* Cristobal Araya Lillo
* Nicolas Silva Paredes