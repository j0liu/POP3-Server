#ifndef CLIENT_DATA_H
#define CLIENT_DATA_H
#include "args.h"
#include "mail.h"
#include "socket_data.h"

#define AUTHORIZATION ((uint8_t)1)
#define TRANSACTION ((uint8_t)2)
#define UPDATE ((uint8_t)4)

typedef struct ClientData {
    SocketData* socket_data;
    uint8_t state;
    struct users* user;
    MailInfo* mail_info_list;
    int mail_count;
    int mail_count_not_deleted;
} ClientData;

ClientData* initialize_client_data(SocketData* socket_data);
void free_client_data(ClientData* client_data);

#endif