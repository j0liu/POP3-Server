#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#include <sys/socket.h>

#include "buffer.h"
#include "pop3.h"
#include "selector.h"
#include "stm/stm.h"

#define N(x) (sizeof(x) / sizeof((x)[0]))
#define AUTHORIZATION ((uint8_t)1)
#define TRANSACTION ((uint8_t)2)
#define UPDATE ((uint8_t)4)
#define INACTIVITY_TIMEOUT 600

void pop3_accept_passive_sockets(struct selector_key* key);
void dajt_accept_passive_sockets(struct selector_key* key);

enum pop3_state {
    WELCOME,
    COMMAND_READ,
    COMMAND_WRITE,
    COMMAND_PROCESSING,
    DONE,
    ERROR
};

/* Used by read and write */
/* typedef struct CommandState {
    buffer *rb, *wb;
    // struct hello_parser parser; 
} CommandState; */

static const struct state_definition client_statbl[] = {
    {
        .state = WELCOME,
        .on_arrival = welcome_init,
        .on_departure = welcome_close,
        .on_write_ready = welcome_write,
    },
    {
        .state = COMMAND_READ,
        .on_arrival = command_read_arrival,
        .on_read_ready = command_read,
    },
    {
        .state = COMMAND_WRITE,
        .on_arrival = command_write_arrival,
        .on_write_ready = command_write,
    },
    {
        .state = COMMAND_PROCESSING,
        .on_arrival = open_mail,
        .on_write_ready = command_processing_write,
    },
    {
        .state = DONE,
        .on_arrival = done_arrival,
    },
    {
        .state = ERROR,
        .on_arrival = command_write_arrival,
        .on_write_ready = error_write
    },
};

#endif