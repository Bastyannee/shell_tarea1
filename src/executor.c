#include "executor.h"
#include "builtins.h"
#include "jobs.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void close_pipes(int pipes[][2], int count) {
    for (int i = 0; i < count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

/*
 * Redirecciones de archivo (R3): open() + dup2() + close() del fd original.
 * Se aplica DESPUES de conectar los pipes, asi "cmd > f" en un pipeline
 * tiene precedencia sobre el pipe para ese descriptor.
 */
static int apply_redirections(const Command *cmd) {
    if (cmd->input_file) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "mishell: %s: %s\n", cmd->input_file, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2");
            close(fd);
            return -1;
        }
        if (fd != STDIN_FILENO) close(fd);
    }

    if (cmd->output_file) {
        int flags = O_WRONLY | O_CREAT | (cmd->append_output ? O_APPEND : O_TRUNC);
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "mishell: %s: %s\n", cmd->output_file, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(fd);
            return -1;
        }
        if (fd != STDOUT_FILENO) close(fd);
    }
    return 0;
}

/* Built-in solitario: corre en la shell; si hay redirecciones se restauran despues. */
static void run_builtin_in_shell(Command *cmd) {
    int saved_in = -1, saved_out = -1;

    fflush(stdout);
    if (cmd->input_file)  saved_in  = dup(STDIN_FILENO);
    if (cmd->output_file) saved_out = dup(STDOUT_FILENO);

    if (apply_redirections(cmd) == 0) {
        execute_builtin(cmd, 1);
        fflush(stdout);
    }

    if (saved_in >= 0)  { dup2(saved_in, STDIN_FILENO);   close(saved_in); }
    if (saved_out >= 0) { dup2(saved_out, STDOUT_FILENO); close(saved_out); }
}

/*
 * Codigo del proceso hijo i de un pipeline de n comandos. Nunca retorna.
 */
static void run_child(Command *cmd, int idx, int n, int pipes[][2],
                      int background, pid_t pgid, const sigset_t *oldmask)
    __attribute__((noreturn));

static void run_child(Command *cmd, int idx, int n, int pipes[][2],
                      int background, pid_t pgid, const sigset_t *oldmask) {
    /* R6: los background van a su propio grupo de procesos, asi el Ctrl+C del
     * terminal (enviado al grupo de foreground) no les llega.
     * pgid == 0 -> el hijo se convierte en lider de un grupo nuevo. */
    if (background) setpgid(0, pgid);

    /* La shell ignora SIGINT/SIGQUIT y una disposicion SIG_IGN sobrevive a
     * exec(): hay que restaurar SIG_DFL para que Ctrl+C mate al hijo. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
    sigprocmask(SIG_SETMASK, oldmask, NULL);   // la shell bloqueo SIGCHLD antes del fork

    /* R4: conectar pipes y cerrar TODOS los extremos en este proceso */
    if (idx > 0)     dup2(pipes[idx - 1][0], STDIN_FILENO);
    if (idx < n - 1) dup2(pipes[idx][1], STDOUT_FILENO);
    close_pipes(pipes, n - 1);

    /* R3: redirecciones de archivo (tienen precedencia sobre los pipes) */
    if (apply_redirections(cmd) < 0) _exit(1);

    /* Un job en background no debe pelear por el terminal: stdin = /dev/null */
    if (background && idx == 0 && !cmd->input_file) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            if (fd != STDIN_FILENO) close(fd);
        }
    }

    if (is_builtin(cmd->argv[0])) {
        int rc = execute_builtin(cmd, 0);
        fflush(stdout);
        _exit(rc == 0 ? 0 : 1);
    }

    execvp(cmd->argv[0], cmd->argv);
    fprintf(stderr, "mishell: %s: %s\n", cmd->argv[0], strerror(errno));
    _exit(127);
}

int execute_pipeline(Pipeline *pl) {
    if (pl->count == 0) return 0;
    const int n = pl->count;

    /* Caso 1: built-in solitario en foreground -> en el proceso de la shell */
    if (n == 1 && !pl->is_background && is_builtin(pl->commands[0].argv[0])) {
        run_builtin_in_shell(&pl->commands[0]);
        return 0;
    }

    int pipes[MAX_CMDS][2];
    pid_t pids[MAX_CMDS];

    /* Bloqueamos SIGCHLD desde antes del fork hasta terminar de esperar /
     * registrar el job. Sin esto:
     *  - el handler podria recolectar (waitpid(-1)) a un hijo de foreground
     *    antes que nuestro waitpid(pid) y este fallaria con ECHILD;
     *  - un job muy corto podria terminar antes de estar en la lista. */
    sigset_t chld, oldmask;
    sigemptyset(&chld);
    sigaddset(&chld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &chld, &oldmask);

    /* N comandos -> N-1 pipes */
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            close_pipes(pipes, i);
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
            return 0;
        }
    }

    fflush(NULL);   // evita que los hijos hereden (y dupliquen) buffers de stdio

    pid_t pgid = 0;
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close_pipes(pipes, n - 1);
            for (int j = 0; j < i; j++) {
                kill(pids[j], SIGTERM);
                waitpid(pids[j], NULL, 0);
            }
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
            return -1;   // error fatal: main terminara la shell
        }
        if (pid == 0) {
            run_child(&pl->commands[i], i, n, pipes, pl->is_background, pgid, &oldmask);
        }
        pids[i] = pid;
        if (pl->is_background) {
            if (i == 0) pgid = pid;
            setpgid(pid, pgid);   // tambien en el padre: evita la carrera con el hijo
        }
    }

    /* La shell cierra TODOS los extremos: si quedara abierto un extremo de
     * escritura aqui, ningun lector veria EOF y el pipeline se colgaria. */
    close_pipes(pipes, n - 1);

    if (pl->is_background) {
        Job *job = jobs_add(pids, n, pgid, pl->cmdline);
        if (job) {
            printf("[%d] %d\n", job->id, (int)pgid);
            fflush(stdout);
        }
    } else {
        int last_status = 0;
        int sigint_seen = 0;
        for (int i = 0; i < n; i++) {
            int st = 0;
            pid_t r;
            do {
                r = waitpid(pids[i], &st, 0);
            } while (r < 0 && errno == EINTR);

            if (i == n - 1) last_status = st;
            if (WIFSIGNALED(st) && WTERMSIG(st) == SIGINT) sigint_seen = 1;
        }

        if (sigint_seen) {
            printf("\n");
        } else if (WIFSIGNALED(last_status) && WTERMSIG(last_status) != SIGPIPE) {
            fprintf(stderr, "%s\n", strsignal(WTERMSIG(last_status)));
        }
        fflush(stdout);
    }

    sigprocmask(SIG_SETMASK, &oldmask, NULL);   // aqui se entrega la SIGCHLD pendiente
    return 0;
}
