#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include "client.h"

static ClientData* initialize_pop3_client_data()
{
    ClientData* client_data = malloc(sizeof(*client_data));
    if (client_data == NULL) {
        return NULL;
    }
    client_data->mail_info_list = NULL;
    client_data->mail_count = 0;
    client_data->mail_count_not_deleted = 0;
    client_data->byte_stuffing = false;
    client_data->current_mail = NO_EMAIL;
    client_data->mail_fd = -1;
    buffer_init(&client_data->mail_buffer, 1024, malloc(1024)); // TODO: constantes
    return client_data;
}




void free_client_data(ClientData* client_data)
{
    // free_socket_data(client_data->socket_data);
    free(client_data);
}

Client* new_client(int client_fd, struct sockaddr_in6* client_addr, socklen_t client_addr_len, bool pop3)
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
        .states = pop3 ? client_statbl_pop3 : client_statbl_dajt
    };

    client->stm = stm;

    stm_init(&client->stm);

    extern parser_definition pop3_parser_definition;
    client->pop3parser = parser_init(&pop3_parser_definition);

    client->socket_data = initialize_socket_data(client_fd); 
    client->state = AUTHORIZATION;
    client->user = NULL;
    client->last_activity_time = time(NULL);

    client->command_state.command_index = -1;
    client->command_state.argLen = 0;
    client->command_state.finished = true;

    if (pop3) {
        client->client_data = initialize_pop3_client_data();
        if (client->client_data == NULL) {
            //Free de collection quizas?
            free(client);
            return NULL;
        }
    }

    return client;
}

void free_client(Client* client)
{
    free_client_data(client->client_data);
    free(client->connection);
    free(client->socket_data);
    parser_destroy(client->pop3parser);
    free(client);
    
}