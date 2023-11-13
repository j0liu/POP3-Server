#ifndef POP3_UTILS_H
#define POP3_UTILS_H

#include "args.h"
#include "client_data.h"
#include "parser/parser.h"
#include <sys/socket.h>

#define N(x) (sizeof(x) / sizeof((x)[0]))

typedef enum command_id {
    CAPA,
    NOOP,
    USER,
    PASS,
    STAT,
    LIST,
    RETR,
    DELE,
    RSET,
    QUIT
    // APOP,
    // TOP,
    // UIDL
} command_id;

typedef struct CommandDescription {
    command_id id;
    char* name;
    bool (*handler)(ClientData* client_data, char* commandParameters, uint8_t parameters_length);
    uint8_t valid_states;
} CommandDescription;

/**
 * estructura utilizada para transportar datos entre el hilo
 * que acepta sockets y los hilos que procesa cada conexión
 */
struct connection {
    int fd;
    socklen_t addrlen;
    struct sockaddr_in6 addr;
};

int serve_pop3_concurrent_blocking(const int server, Args* args);

#endif