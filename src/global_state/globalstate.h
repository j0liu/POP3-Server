#ifndef GLOBALSTATE_H
#define GLOBALSTATE_H

#include <stdlib.h>
#include <stdbool.h>
#include "../client.h"
#define MAX_CLIENTS 512
typedef struct GlobalState {
    unsigned long total_connections;
    unsigned long current_connections;
    unsigned long total_bytes_sent;
    bool transformations_enabled;
    char * transformation_path;
    size_t transformation_path_len;
    unsigned current_buffer_size;
    Client * clients[MAX_CLIENTS];
    unsigned total_clients;
} GlobalState;

void set_transformation(char *raw_transformation);
bool add_client(Client * client);
void remove_client(Client * client);
#endif