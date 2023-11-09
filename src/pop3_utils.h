#ifndef POP3_UTILS_H
#define POP3_UTILS_H

#include <sys/socket.h>
#include "parser/parser.h"
#include "client_data.h"
#include "args.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

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
    char * name;
    void (*handler)(ClientData * client_data, char * commandParameters, uint8_t parameters_length);
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

int consume_pop3_buffer(parser * pop3parser, ClientData * client_data, ssize_t n);
int process_event(parser_event * event, ClientData * client_data);
int serve_pop3_concurrent_blocking(const int server, Pop3Args * popargs);


#endif