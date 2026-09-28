#include "parser.h"
#include <stdio.h>
#include <string.h>

typedef enum { T_WORD, T_PIPE, T_IN, T_OUT, T_APPEND, T_BG } TokType;

typedef struct {
    TokType type;
    char *text;   // solo para T_WORD (apunta dentro de pipeline->buf)
} Token;

#define MAX_TOKENS (MAX_ARGS * MAX_CMDS)

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int is_op(char c) {
    return c == '|' || c == '<' || c == '>' || c == '&';
}

static void add_op(Token *toks, int *n, TokType type) {
    toks[*n].type = type;
    toks[*n].text = NULL;
    (*n)++;
}

/*
 * Tokenizador propio. Reconoce palabras, comillas simples/dobles y los
 * operadores | < > >> & aunque no esten separados por espacios ("ls|wc").
 * Las palabras se copian (sin comillas) a buf, separadas por '\0'.
 * Devuelve la cantidad de tokens o -1 si hay error.
 */
static int tokenize(const char *line, char *buf, size_t bufsz, Token *toks, int max) {
    const char *r = line;
    char *w = buf;
    char *end = buf + bufsz;
    int n = 0;

    while (*r) {
        while (is_space(*r)) r++;
        if (*r == '\0') break;

        if (n >= max) {
            fprintf(stderr, "mishell: demasiados tokens\n");
            return -1;
        }

        if (*r == '|') { add_op(toks, &n, T_PIPE); r++; continue; }
        if (*r == '<') { add_op(toks, &n, T_IN);   r++; continue; }
        if (*r == '&') { add_op(toks, &n, T_BG);   r++; continue; }
        if (*r == '>') {
            if (r[1] == '>') { add_op(toks, &n, T_APPEND); r += 2; }
            else             { add_op(toks, &n, T_OUT);    r += 1; }
            continue;
        }

        /* Palabra: puede mezclar texto y tramos entre comillas */
        char *start = w;
        while (*r && !is_space(*r) && !is_op(*r)) {
            if (*r == '"' || *r == '\'') {
                char q = *r++;
                while (*r && *r != q) {
                    if (w + 2 > end) goto too_long;
                    *w++ = *r++;
                }
                if (*r != q) {
                    fprintf(stderr, "mishell: comillas sin cerrar\n");
                    return -1;
                }
                r++;
            } else {
                if (w + 2 > end) goto too_long;
                *w++ = *r++;
            }
        }
        if (w + 1 > end) goto too_long;
        *w++ = '\0';
        toks[n].type = T_WORD;
        toks[n].text = start;
        n++;
    }
    return n;

too_long:
    fprintf(stderr, "mishell: linea demasiado larga\n");
    return -1;
}

static void trim_right(char *s) {
    size_t len = strlen(s);
    while (len > 0 && is_space(s[len - 1])) s[--len] = '\0';
}

int parse_line(char *line, Pipeline *pipeline) {
    memset(pipeline, 0, sizeof(Pipeline));

    line[strcspn(line, "\n")] = '\0';

    Token toks[MAX_TOKENS];
    int nt = tokenize(line, pipeline->buf, sizeof(pipeline->buf), toks, MAX_TOKENS);
    if (nt < 0) return -1;
    if (nt == 0) return 0;

    Command *cur = &pipeline->commands[0];
    int ncmd = 1;

    for (int i = 0; i < nt; i++) {
        switch (toks[i].type) {
        case T_WORD:
            if (cur->argc >= MAX_ARGS - 1) {
                fprintf(stderr, "mishell: demasiados argumentos\n");
                return -1;
            }
            cur->argv[cur->argc++] = toks[i].text;   // argv[argc] queda en NULL (memset)
            break;

        case T_PIPE:
            if (cur->argc == 0) {
                fprintf(stderr, "mishell: error de sintaxis cerca de '|'\n");
                return -1;
            }
            if (ncmd >= MAX_CMDS) {
                fprintf(stderr, "mishell: demasiados comandos en el pipeline (max %d)\n", MAX_CMDS);
                return -1;
            }
            cur = &pipeline->commands[ncmd++];
            break;

        case T_IN:
        case T_OUT:
        case T_APPEND: {
            if (i + 1 >= nt || toks[i + 1].type != T_WORD) {
                fprintf(stderr, "mishell: error de sintaxis: falta el archivo de redireccion\n");
                return -1;
            }
            TokType op = toks[i].type;
            char *file = toks[++i].text;
            if (op == T_IN) {
                cur->input_file = file;
            } else {
                cur->output_file = file;
                cur->append_output = (op == T_APPEND);
            }
            break;
        }

        case T_BG:
            if (i != nt - 1) {
                fprintf(stderr, "mishell: '&' solo puede ir al final de la linea\n");
                return -1;
            }
            pipeline->is_background = 1;
            break;
        }
    }

    if (cur->argc == 0) {
        fprintf(stderr, "mishell: error de sintaxis: comando vacio\n");
        return -1;
    }

    /* Texto para jobs/pmon: la linea sin espacios extremos y sin el '&' final */
    const char *s = line;
    while (is_space(*s)) s++;
    strncpy(pipeline->cmdline, s, sizeof(pipeline->cmdline) - 1);
    trim_right(pipeline->cmdline);
    if (pipeline->is_background) {
        size_t len = strlen(pipeline->cmdline);
        if (len > 0 && pipeline->cmdline[len - 1] == '&') pipeline->cmdline[len - 1] = '\0';
        trim_right(pipeline->cmdline);
    }

    pipeline->count = ncmd;
    return ncmd;
}

void free_pipeline(Pipeline *pipeline) {
    // Los tokens viven dentro de pipeline->buf: no hay memoria dinamica que liberar.
    (void)pipeline;
}
