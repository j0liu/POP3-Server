#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_USERS 10
#define MAX_ADMINS 5
#define MAX_TRANFORMATION_PATH_LEN 256

struct User {
    char* name;
    char* pass;
};

typedef struct Args {
    unsigned short pop3_port;
    unsigned short dajt_port;
    char* maildir_path;
    struct User users[MAX_USERS];
    struct User admins[MAX_ADMINS];
    size_t quantity_users;
    size_t quantity_admins;
    char * transformation_path;
} Args;

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void parse_args(const int argc, char** argv, Args* args);

#endif
