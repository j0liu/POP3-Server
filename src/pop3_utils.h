#ifndef POP3_UTILS_H
#define POP3_UTILS_H

#include "socket_data.h"

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
    void (*handler)(SocketData * socket_data);
} command_description;

#endif