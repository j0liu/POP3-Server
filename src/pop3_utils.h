#ifndef POP3_UTILS_H
#define POP3_UTILS_H

#include "socket_data.h"
#include "parser/parser.h"
#include "args.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

typedef enum command_id {
    CAPA,
    USER,
    PASS,
    STAT,
    LIST,
    RETR,
    DELE,
    NOOP,
    RSET,
    QUIT
    // APOP,
    // TOP,
    // UIDL
} command_id;

typedef struct command_description {
    command_id id;
    char * name;
    void (*handler)(SocketData * socket_data, char * commandParameters, uint8_t parameters_length);
} command_description;

/**
 * estructura utilizada para transportar datos entre el hilo
 * que acepta sockets y los hilos que procesa cada conexión
 */
struct connection {
  int fd; 
  socklen_t addrlen;
  struct sockaddr_in6 addr;
};

int consume_pop3_buffer(parser * pop3parser, SocketData * socket_data, ssize_t n);
int process_event(parser_event * event, SocketData * socket_data);
int serve_pop3_concurrent_blocking(const int server, Pop3Args * popargs);


#endif