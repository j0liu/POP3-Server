#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_USERS 10

struct users {
    char* name;
    char* pass;
};

typedef struct Args {
    unsigned short pop3_port;
    unsigned short dajt_port;
    char* maildir_path;
    struct users users[MAX_USERS];
    size_t quantity_users;
} Args;

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void parse_args(const int argc, char** argv, Args* args);

#endif
