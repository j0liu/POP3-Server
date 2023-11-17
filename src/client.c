#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include "client.h"

ClientData* initialize_client_data(SocketData* socket_data)
{
    if (socket_data == NULL) {
        return NULL;
    }
    ClientData* client_data = malloc(sizeof(*client_data));
    if (client_data == NULL) {
        return NULL;
    }
    client_data->socket_data = socket_data;
    client_data->state = AUTHORIZATION;
    client_data->user = NULL;
    client_data->mail_info_list = NULL;
    client_data->mail_count = 0;
    client_data->mail_count_not_deleted = 0;
    client_data->last_activity_time = time(NULL);
    return client_data;
}

void free_client_data(ClientData* client_data)
{
    free_socket_data(client_data->socket_data);
    free(client_data);
}

Client* new_client(int client_fd, struct sockaddr_in6* client_addr, socklen_t client_addr_len)
{
    Client* client = malloc(sizeof(Client));
    if (client == NULL) {
        return NULL;
    }
    client->connection = malloc(sizeof(Connection));
    client->connection->fd = client_fd;
    client->connection->addrlen = client_addr_len;
    memcpy(&client->connection->addr, client_addr, client_addr_len);

    struct state_machine stm = {
        .initial = WELCOME,
        .max_state = ERROR,
        .states = client_statbl,
    };

    client->stm = stm;

    stm_init(&client->stm);

    client->client_data = initialize_client_data(initialize_socket_data(client_fd));
    if (client->client_data == NULL) {
        //Free de collection quizas?
        free(client);
        return NULL;
    }

    return client;
}

void free_client(Client* client)
{
    free_client_data(client->client_data);
    free(client->connection);
    free(client);
}