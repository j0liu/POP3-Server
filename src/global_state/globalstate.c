#include "globalstate.h"
#include <string.h>
#include "../logger/logger.h"

GlobalState global_state = {
    .current_connections = 0,
    .total_connections = 0,
    .total_bytes_sent = 0,
    .transformation_path = NULL, 
    .transformation_path_len = 0,
    .transformations_enabled = false,
    .current_buffer_size = 4096,
    .clients = {0}, 
    .total_clients = 0
};

bool add_client(Client * client) {
    if (client == NULL) return false;

    if (global_state.total_clients >= MAX_CLIENTS)
        return false;

    for(int i = global_state.total_clients % MAX_CLIENTS; i < MAX_CLIENTS; i = (i + 1) % MAX_CLIENTS) {
        if (global_state.clients[i] == NULL) {
            global_state.clients[i] = client;
            client->client_index = i;
            global_state.total_clients++;  
            return true;
        }
    }
    return false;
}

void remove_client(Client * client) {
    if (client == NULL) return;
    if (global_state.clients[client->client_index] != NULL) {
        logf(LOG_INFO, "Removing client %d...", client->client_index);
        global_state.clients[client->client_index] = NULL;
        global_state.total_clients--;
        int client_index = client->client_index;
        free_client(client);
        logf(LOG_INFO, "Client %d removed", client_index);
    }
}

void set_transformation(char *raw_transformation) {
    size_t len = strlen(raw_transformation) + 1;
    global_state.transformation_path_len = len;

    if (global_state.transformation_path == NULL) {
        global_state.transformation_path = malloc(len * sizeof(char));
    } else {
        global_state.transformation_path = realloc(global_state.transformation_path, len * sizeof(char));
    }
    strcpy(global_state.transformation_path, raw_transformation);
}
