#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#include <sys/socket.h>

#include "args.h"
#include "client_data.h"

#define N(x) (sizeof(x) / sizeof((x)[0]))

typedef struct CommandDescription {
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

#endif