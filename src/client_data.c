#include "client_data.h"
#include "socket_data.h"

#include <stdlib.h>

ClientData* initialize_client_data(SocketData* socket_data)
{
    ClientData* client_data = malloc(sizeof(*client_data));
    client_data->socket_data = socket_data;
    client_data->state = AUTHORIZATION;
    client_data->user = NULL;
    client_data->mail_info_list = NULL;
    client_data->mail_count = 0;
    client_data->mail_count_not_deleted = 0;
    return client_data;
}

void free_client_data(ClientData* client_data)
{
    free_socket_data(client_data->socket_data);
    free(client_data);
}